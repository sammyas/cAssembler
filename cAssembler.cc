#include <string> 
#include <stdlib.h>
#include "cadvisor-sources.h"
#include <chrono>
#include <vector> 
#include <map>
#include <set>
#include <iostream>
#include <curl/curl.h>
#include <thread> 
#include <mutex>
#include <limits>
using namespace std;
static const int PORT = 8080;
static const string MEMORY= "mem";
static const string CPU="cpu";
static const int CONTAINER_UPDATE_FREQUENCY=60; 
static const int INFLUXDB_UPDATE_FREQUENCY=60; 
static const string USERNAME="root";
static const string PASSWORD="root";
static const string INFLUXDB_POINTS= "\",\n \"columns\": [\"sequence_number\", \"slave_ip\",  \"memory\", \"cpu\",  \"timestamp\"],\n \"points\": [\n ";
static mutex oslock;
void findcontainers(string data, struct cAdvisorStats *stats);
string retrieveCadvisorAPI(string address);

/* searches for the next occurence of the string before 
* and returns the string contained between before and after,
* or "" if no such string exists
*/ 
string findData(string data, string before, string after, size_t &pos){
	size_t start = data.find(before, pos);
        if(start==string::npos){
                 return "";
        }
	start+=string(before).length();
        size_t end = data.find(after, start);
	pos=end; 
        string stat = data.substr(start, end-start);
     	return stat; 
}

/* given a string of data from the CAdvisor API, it parses the 
*  data into a vector of statistics containing the memory, cpu and timestamp
*/
vector<struct Stat> parseCurrentStats(string data){
	size_t pos=0;
        double  prevMem=0;
	vector<struct Stat> stats;
	while(true){
		 string timestamp = findData(data, "timestamp\":\"", "\"",  pos);
                 if(timestamp.length()==0) break;
                 string cpu=findData(data, "\"cpu\":{\"usage\":{\"total\":", ",", pos);
                 if(cpu.length()==0) break;
                 double cpustat=stod(cpu);
        	 string memory=findData(data, "memory\":{\"usage\":", ",", pos);
		 if(memory.length()==0) break; 
		 double memstat=stod(memory);
		 if(prevMem!=0){
 	  		 struct Stat new_stat;
		   	 new_stat.values[MEMORY]=memstat;
			 new_stat.values[CPU]=cpustat-prevMem;
			 new_stat.timestamp=timestamp;
			 stats.push_back(new_stat);
		}
		prevMem=cpustat;
	 }
	return stats;
}
        
/*
* Given a cAdvisorStats and the name of a container in the cAsdvisor instance, 
* continually retrieves and parses the statistics, storing them in the cAdvisor struct 
* It then sleeps for 60 seconds until new statistics become available. 
*/
void retrieveContainerStats(string name, struct cAdvisorStats* stats){
	string address = stats->slave_ip + ":" + to_string(stats->port) + "/api/v1.1/containers/" + name;
	while(true){
		string data = retrieveCadvisorAPI(address);
		if(data.length()==0){
			break; 
		}
	        vector<struct Stat> curr_stats=parseCurrentStats(data);
		if(curr_stats.size()>0){
 	                cout << "Just parsed stats for " + name << endl;
			stats->lock->lock();
			stats->containers[name].stats.insert(stats->containers[name].stats.end(), curr_stats.begin(), curr_stats.end());
			stats->lock->unlock();
			this_thread::sleep_for(chrono::seconds(60));
		}
	}
	stats->lock->lock();
        stats->containers.erase(name);
	cout << "Removing container " << name << endl;
	stats->lock->unlock();
}

/*
* helper function for the retrieveCadvisorAPI function 
*/
static size_t getcAdvisorData(char *ptr, size_t size, size_t nmemb, string *s){
	s->append(ptr, size*nmemb); 
	return size*nmemb;
}

/*
* given a container, returns the data currently stored in that container as 
* a string json that can be parsed by the influxdb databate. 
*/
string jsonContainerStats(struct Container * container, string slave_ip){
	string json = "{ \n \"name\": \"" + container->name + INFLUXDB_POINTS;
	for(size_t i=0; i<container->stats.size(); i++){
		struct Stat *cur_stat= &(container->stats[i]);
		string curjson="[" + to_string(i+1) + ", \"" + slave_ip + +"\", " +  to_string(cur_stat->values[MEMORY]) +", " + to_string(cur_stat->values[CPU]) + ", \"" + (cur_stat->timestamp) +"\"],\n";
		json+=curjson; 
	}
	if(json.compare("{ \n \"name\": \"" + container->name + INFLUXDB_POINTS)==0){
		return "";
	}
        container->stats.clear();
	json.erase(json.length()-2, 2);
	json+="\n]\n },\n ";
	return json;
}
/*
* collects all stats from all containers currently stored 
* in the cAdvisorStat and formats them into a json to send to influxdb. 	
*/
void sendStats(struct cAdvisorStats *stats){
	while(true){
		this_thread::sleep_for(chrono::seconds(INFLUXDB_UPDATE_FREQUENCY)); 
		stats->lock->lock(); 
		string json="[\n";
		for( auto i=stats->containers.begin(); i!=stats->containers.end(); i++){
			string currentStats=jsonContainerStats(&i->second, stats->slave_ip); 
			string name = "got stats for: " + i->first; 
		 	cout << name << endl;
			json+=currentStats;
		}
		stats->lock->unlock(); 
                if(json.compare("[\n")==0) continue;
	        json.erase(json.length()-3, 3);
        	json+="\n]";
		cout << "The json is: " << json;
		string command = "curl -X  POST -d '" + json + "' 'http://10.79.6.70:8086/db/Mesos/series?u=" + USERNAME + "&p=" + PASSWORD + "'";
	        int ret = system(command.c_str());
 	        if(ret!=0){
        	        cout << "Failed to contact Influxdb" << endl;
        	}
	}
}
/*
* searches the cAdvisor API associated with the given slave cAdvisor instance 
* launches a thread to send information to influxdb for this slave
* Then, monitors the docker containers currently running on the given slave 
* and parses the statistics for each. 
*/
void retrieveCadvisorContainers(struct cAdvisorStats *stats){
	string address =stats->slave_ip + ":" + to_string(stats->port) + "/api/v1.1/containers/docker";
	thread s=thread(sendStats, stats); 
	s.detach();
	while(true){
	        string data = retrieveCadvisorAPI(address);
		findcontainers(data, stats);
                this_thread::sleep_for(chrono::seconds(CONTAINER_UPDATE_FREQUENCY));

	} 
}

/* 
* uses the libcurl interface to retrieve data from the given address 
* and returns that data in the form of a string
*/
string retrieveCadvisorAPI(string address){
	CURL *curl;
	CURLcode res; 
	curl =curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &getcAdvisorData);
	string data;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	if(curl){
		curl_easy_setopt(curl, CURLOPT_URL, address.c_str());
		res = curl_easy_perform(curl);
		if(res != CURLE_OK){
			return "";
		}
		curl_easy_cleanup(curl);
	}
	return data;
}
/*
* locates the containers currently active in the given slave, 
* and launches a new thread to monitor any container that is 
* not already being monitored. 
*/
void findcontainers(string data, struct cAdvisorStats *stats){
	size_t pos=0;
	while(true){
		string name =findData(data, "name\":\"/", "\"", pos);
		if(name.length()==0) break; 
                if(name.compare("docker")==0) continue;
		if(stats->containers.find(name)==stats->containers.end()){
	               stats->lock->lock();
	                cout << "A new container: " << name << endl;
			struct Container container;
			container.name=name;
//FIX DATA STRUCTUER HERERERE!!!!!! 
			stats->containers[name]=container;
			stats->lock->unlock();
			thread t=thread(retrieveContainerStats, name, stats); 
			t.detach();
		}
	}
	
}
/*
* creates a cAdvisorSources instance, containing one cAdvisorStats instance 
* for every slave currently running in mesos. 
*/
cAdvisorSources makecAdvisorSources(vector<string> &slaveIPs, int port){
	cAdvisorSources csources;
	csources.num_slaves=0; 
	csources.slave_stats = map<string, cAdvisorStats>(); 
	csources.port=port; 
        for(string & ip: slaveIPs){
		csources.num_slaves++;
                struct cAdvisorStats stats;
                stats.port=port;
                stats.slave_ip=ip;
		unique_ptr<mutex> lock(new mutex); 
		stats.lock=move(lock);
                stats.containers=map<string, struct Container>();
                csources.slave_stats[ip]=move(stats);
        }
	return csources;
}

/*
* uses the Mesos Webiu to retrieve the currently running slaves 
*/
vector<string> getSlaveIPs(string master){
	string address=master+":5050/master/state.json";
	string data=retrieveCadvisorAPI(address);
	if(data.length()==0){
		vector<string> null; 
		return null;
	}
	vector<string> slaves;
	size_t pos=0; 
	while(true){
		string slave = findData(data, "slave(1)@", ":",  pos);
		if(slave.length()==0) break;
		slaves.push_back(slave);
	}
	return slaves; 
}

int main(int argc, char* argv[]){
	if(argc!=2){
		cout << "Please enter the IP address of the mesos master." << endl; 
		return -1;
	}
	vector<string> slaveIPs=getSlaveIPs(argv[1]);
	if(slaveIPs.size()==0){
		cout << "Could not contact master: " << argv[1] << endl;
		return -1;
	} 
	cout << "The IP addresses of the currently running slaves are: " << endl;
	for(string & slave: slaveIPs){
		cout << slave << "," << endl;
	}
	cAdvisorSources sources = makecAdvisorSources(slaveIPs, PORT);
	vector<thread> slaves;
	for(auto i=sources.slave_stats.begin(); i!=sources.slave_stats.end(); i++){
		slaves.push_back(thread(retrieveCadvisorContainers, &(i->second))); 
	}
	for(auto &t: slaves){
		t.join();
		}
}

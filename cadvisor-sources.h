/* File: cAdvisorSources.h 
* -----------------------------------------
* Aggregates data from individual cAdvisors and ships data to influxDB
*/

/**
 * Type: cAdvisorSource
 * -------------
 */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <utility>

struct Stat{
        std::map<std::string, double> values;
        std::string timestamp;
};

//for storage purposes, two stats are defined to be the same if they happened in at the same time
inline bool operator<(const Stat& one, const Stat& two) {
        return one.timestamp < two.timestamp;
}

struct Container{
        std::string name;
        std::vector<Stat> stats;

};

inline bool operator<(const Container& one, const Container& two) {
  return one.name < two.name;
}

struct cAdvisorStats{
        std::string slave_ip;
        std::unique_ptr<std::mutex> lock;
        std::string slave_name;
        std::map<std::string, struct Container> containers;
        int port;
};

inline bool operator<(const cAdvisorStats& one, const cAdvisorStats& two) {
  return one.slave_ip < two.slave_ip;
}


struct cAdvisorSources{
	std::map<std::string, cAdvisorStats> slave_stats; 
	int num_slaves; 
	int port; 
};



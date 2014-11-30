/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <libgen.h>
#include <set>
#include <iostream>
#include <string>

#include <boost/lexical_cast.hpp>

#include <mesos/scheduler.hpp>

#include <stout/exit.hpp>
#include <stout/os.hpp>

using namespace mesos;

using boost::lexical_cast;

using std::cout;
using std::cerr;
using std::endl;
using std::flush;
using std::string;
using std::vector;

const int32_t CPUS_PER_TASK = 1;
const int32_t MEM_PER_TASK = 32;

class DockerNoExecutorScheduler : public Scheduler
{
public:
  DockerNoExecutorScheduler()
    : current_slaves(), tasksLaunched(0), tasksFinished(0), totalTasks(1) {}

  virtual ~DockerNoExecutorScheduler() {}

  virtual void registered(SchedulerDriver*,
                          const FrameworkID&,
                          const MasterInfo&)
  {
    cout << "Registered!" << endl;
  }

  virtual void reregistered(SchedulerDriver*, const MasterInfo& masterInfo) {}

  virtual void disconnected(SchedulerDriver* driver) {}

  virtual void resourceOffers(SchedulerDriver* driver,
                              const vector<Offer>& offers)
  {
    cout << "." << flush;
    for (size_t i = 0; i < offers.size(); i++) {
      const Offer& offer = offers[i];
      double cpus = 0;
      double mem = 0;
      if(current_slaves.find(offer.hostname())!=current_slaves.end()){
		cout << "rejecting offer on host " << offer.hostname() << "Because cAdvisor has already launched here." << endl;
		continue; 
	}
	current_slaves.insert(offer.hostname());
      for (int i = 0; i < offer.resources_size(); i++) {
        const Resource& resource = offer.resources(i);
        if (resource.name() == "cpus" &&
            resource.type() == Value::SCALAR) {
          cpus = resource.scalar().value();
        } else if (resource.name() == "mem" &&
                   resource.type() == Value::SCALAR) {
          mem = resource.scalar().value();
        }
      }

      // Launch tasks.
  
      if(cpus >= CPUS_PER_TASK && mem >= MEM_PER_TASK) {
       vector<TaskInfo> tasks;
        int taskId = tasksLaunched++;
        current_slaves.insert(offer.hostname());
        cout << "Starting task " << taskId << " on "
             << offer.hostname() << endl;

        TaskInfo task;
        task.set_name("Task " + lexical_cast<string>(taskId));
        task.mutable_task_id()->set_value(lexical_cast<string>(taskId));
        task.mutable_slave_id()->MergeFrom(offer.slave_id());
        task.mutable_command()->set_value("while true; sleep 1; done");
	
        // Use Docker to run the task.
        ContainerInfo* container = task.mutable_container(); 
	container->set_type(ContainerInfo::DOCKER);
        container->mutable_docker()->set_image("google/cadvisor");
        Volume* vol1 = container->add_volumes();
	vol1->set_container_path("/rootfs");
	vol1->set_host_path("/");
	vol1->set_mode(Volume::RO);
        Volume* vol2 = container->add_volumes();
        vol2->set_container_path("/var/run");
        vol2->set_host_path("/var/run");
        vol2->set_mode(Volume::RW);
        Volume* vol3 = container->add_volumes();
        vol3->set_container_path("/sys");
	vol3->set_host_path("/sys");
        vol3->set_mode(Volume::RO); 
        Volume* vol4 = container->add_volumes();
        vol4->set_container_path("/var/lib/docker");
        vol4->set_mode(Volume::RO);
	vol4->set_host_path("/var/lib/docker/");
	Resource* resource;

        resource = task.add_resources();
        resource->set_name("cpus");
        resource->set_type(Value::SCALAR);
        resource->mutable_scalar()->set_value(CPUS_PER_TASK);

        resource = task.add_resources();
        resource->set_name("mem");
        resource->set_type(Value::SCALAR);
        resource->mutable_scalar()->set_value(MEM_PER_TASK);

        tasks.push_back(task);
        driver->launchTasks(offer.id(), tasks);

        cpus -= CPUS_PER_TASK;
        mem -= MEM_PER_TASK;
      }
      else{
	cout << "rejecting offer due to insufficient resources." << endl;
	}
      }
  }

  virtual void offerRescinded(SchedulerDriver* driver,
                              const OfferID& offerId) {}

  virtual void statusUpdate(SchedulerDriver* driver, const TaskStatus& status)
  {
    int taskId = lexical_cast<int>(status.task_id().value());

    cout << "Task " << taskId << " is in state " << status.state() << endl;

    if (status.state() == TASK_FINISHED)
      tasksFinished++;

    if (tasksFinished == totalTasks)
      driver->stop();
  }

  virtual void frameworkMessage(SchedulerDriver* driver,
                                const ExecutorID& executorId,
                                const SlaveID& slaveId,
                                const string& data) {}

  virtual void slaveLost(SchedulerDriver* driver, const SlaveID& slaveId) {}

  virtual void executorLost(SchedulerDriver* driver,
                            const ExecutorID& executorId,
                            const SlaveID& slaveId,
                            int status) {}

  virtual void error(SchedulerDriver* driver, const string& message) {}

private:
  std::set<string> current_slaves;
  int tasksLaunched;
  int tasksFinished;
  int totalTasks;
};


int main(int argc, char** argv)
{
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " <master>" << endl;
    return -1;
  }

  DockerNoExecutorScheduler scheduler;

  FrameworkInfo framework;
  framework.set_user(""); // Have Mesos fill in the current user.
  framework.set_name("Docker No Executor Framework (C++)");

  if (os::hasenv("MESOS_CHECKPOINT")) {
    cout << "Enabling checkpoint for the framework" << endl;
    framework.set_checkpoint(true);
  }

  MesosSchedulerDriver* driver;
  if (os::hasenv("MESOS_AUTHENTICATE")) {
    cout << "Enabling authentication for the framework" << endl;

    if (!os::hasenv("DEFAULT_PRINCIPAL")) {
      EXIT(1) << "Expecting authentication principal in the environment";
    }

    if (!os::hasenv("DEFAULT_SECRET")) {
      EXIT(1) << "Expecting authentication secret in the environment";
    }

    Credential credential;
    credential.set_principal(getenv("DEFAULT_PRINCIPAL"));
    credential.set_secret(getenv("DEFAULT_SECRET"));

    framework.set_principal(getenv("DEFAULT_PRINCIPAL"));

    driver = new MesosSchedulerDriver(
        &scheduler, framework, argv[1], credential);
  } else {
    framework.set_principal("no-executor-framework-cpp");

    driver = new MesosSchedulerDriver(
        &scheduler, framework, argv[1]);
  }

  int status = driver->run() == DRIVER_STOPPED ? 0 : 1;

  // Ensure that the driver process terminates.
  driver->stop();

  delete driver;
  return status;
}

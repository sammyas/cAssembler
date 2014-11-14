/**
* File: cAdvisorStats
* holds data about each of the docker containers tracked by a given cAdvisor instance
*/
#pragma once
#include <string> 
#include <vector>
#include <set>
#include "container.h"
#include <mutex> 
#include <memory>
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


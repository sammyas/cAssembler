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
#include "cadvisor-stats.h"
#include "container.h"

struct cAdvisorSources{
	std::map<std::string, cAdvisorStats> slave_stats; 
	int num_slaves; 
	int port; 
};


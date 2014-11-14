#pragma once
#include <string> 
#include <map>
#include <utility>

struct Stat{
	double mem;
	double cpu;
	std::string timestamp;
};

struct StatType{
	std::string name; 
	std::vector<Stat> values;
};

inline bool operator<(const StatType& one, const StatType& two) {
	return one.name < two.name; 
}

struct Container{
        std::string name;
	std::map<std::string, struct StatType> stat_types;
};

	


inline bool operator<(const Container& one, const Container& two) {
  return one.name < two.name;
}


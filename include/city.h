#ifndef CITY_H
#define CITY_H
#include <string>
#include <vector>
#include "emergency_service.h"
class City
{
public:
    int id;
    std::string name;
    std::vector<EmergencyService> services;  
    City(int id, const std::string& name)
        : id(id), name(name) {}
};
#endif
#ifndef GRAPH_H
#define GRAPH_H
#include <vector>
#include <string>
#include "edge.h"
#include "city.h"
class Graph
{
public:
    int V;                              
    std::vector<std::vector<Edge>> adj; 
    std::vector<City> cities;           
    Graph(int V, const std::vector<std::string>& cityNames);
    void addEdge(int src, int dest, int distance, int traffic);
    void addEmergencyServices(int cityId,  const std::vector<EmergencyService>& services);
    void printGraph() const;
};
#endif
#include "../include/graph.h"
#include <iostream>
#include <stdexcept>
Graph::Graph(int V, const std::vector<std::string>& cityNames)
    : V(V)
{
    if (static_cast<int>(cityNames.size()) != V)
        throw std::invalid_argument("Please provide exactly one name per city.");
    adj.resize(V);
    for (int i = 0; i < V; i++)
        cities.push_back(City(i, cityNames[i]));
}
void Graph::addEdge(int src, int dest, int distance, int traffic) 
{
    if (src < 0 || src >= V || dest < 0 || dest >= V)
        throw std::out_of_range("City index out of range while adding road.");
    if (distance <= 0)
        throw std::invalid_argument("Road distance must be greater than zero.");
    if (traffic <= 0)
        throw std::invalid_argument("Traffic factor must be at least 1 (1 = clear road).");
    adj[src].push_back(Edge(dest, distance, traffic));
    adj[dest].push_back(Edge(src,  distance, traffic));
}
void Graph::printGraph() const 
{
    std::cout << "\n Road Network \n";
    for (int u = 0; u < V; u++) {
        std::cout << "  " << cities[u].name << "  :  ";
        for (const auto& road : adj[u]) 
        {
            std::cout << "["  << cities[road.destination].name
                      << "  dist:"    << road.distance  << " km"
                      << "  traffic:" << road.traffic
                      << "  cost:"    << road.effectiveWeight()
                      << "]  ";
        }
        std::cout << "\n";
    }
}
void Graph::addEmergencyServices(int cityId,const std::vector<EmergencyService>& services) 
{
    if (cityId < 0 || cityId >= V)
        throw std::out_of_range("City index out of range.");
    cities[cityId].services = services;
}

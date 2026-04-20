#ifndef DASHBOARD_H
#define DASHBOARD_H
#include "graph.h"
#include "dijkstra.h"
#include "graph_coloring.h"
#include "emergency_service.h"
#include <vector>
#include <string>
struct StopInfo
{
    int cityId;
    std::string cityName;
    int zone; 
    int distFromSource;   
    int nextRoadDist;     
    int nextRoadTraffic;  
    int nextRoadCost;     
    std::vector<EmergencyService> services;
};
class Dashboard
{
public:
    std::vector<StopInfo> buildRoute(Graph& g, Dijkstra& d,GraphColoring& gc,int source,int dest,bool useTraffic);
    void printDashboard(const std::vector<StopInfo>& stops) const;
};
#endif
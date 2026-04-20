#include "../include/dashboard.h"
#include <iostream>
#include <climits>
std::vector<StopInfo> Dashboard::buildRoute(Graph& g, Dijkstra& d,GraphColoring& gc,int source,int dest, bool useTraffic)
{
    gc.greedyColoring(g);
    d.shortestPath(g, source, useTraffic);
    std::vector<int> path  = d.getRoute(dest);
    std::vector<int> dists = d.getDist();
    std::vector<StopInfo> stops;
    for (int i = 0; i < (int)path.size(); i++)
    {
        int u = path[i];
        StopInfo s;
        s.cityId = u;
        s.cityName = g.cities[u].name;
        s.zone = gc.color[u];
        s.distFromSource = dists[u];
        s.services = g.cities[u].services;
        if (i + 1 < (int)path.size())
        {
            int next = path[i + 1];
            s.nextRoadDist = -1;
            s.nextRoadTraffic = -1;
            s.nextRoadCost = -1;
            for (const auto& road : g.adj[u])
            {
                if (road.destination == next)
                {
                    s.nextRoadDist = road.distance;
                    s.nextRoadTraffic = road.traffic;
                    s.nextRoadCost = road.effectiveWeight();
                    break;
                }
            }
        }
        else
        {
            s.nextRoadDist = -1;
            s.nextRoadTraffic = -1;
            s.nextRoadCost = -1;
        }
        stops.push_back(s);
    }
    return stops;
}
void Dashboard::printDashboard(const std::vector<StopInfo>& stops) const
{
    std::string trafficLabel[4] = {"", "Clear", "Moderate", "Heavy"};
    std::cout << "  TRAVEL DASHBOARD\n";
    for (int i = 0; i < (int)stops.size(); i++)
    {
        const auto& s = stops[i];
        std::cout << "\n[Stop " << (i + 1) << " of " << stops.size() << "]  "<< s.cityName<< "  |  Zone " << s.zone<< "  |  " << s.distFromSource << " km from start\n";
        if (s.nextRoadDist != -1)
        {
            std::string tl = (s.nextRoadTraffic >= 1 && s.nextRoadTraffic <= 3) ? trafficLabel[s.nextRoadTraffic] : "Unknown";
            std::cout << "  Road ahead  : " << s.nextRoadDist << " km"<< "  |  Traffic: " << tl << " (" << s.nextRoadTraffic << ")"
                      << "  |  Effective cost: " << s.nextRoadCost << "\n";
        }
        else
        {
            std::cout << "  Road ahead  :  (destination reached)\n";
        }
        if (s.services.empty())
        {
            std::cout << "  Emergency services: not loaded for this city\n";
        }
        else
        {
            std::cout << "  Emergency Services:\n";
            std::cout << "  " << std::string(60, '-') << "\n";
            for (const auto& svc : s.services)
            {
                std::cout << "  [" << svc.type << "]\n"<< "    Number  : " << svc.number   << "\n"<< "    Station : " << svc.station  << "\n"
                          << "    Address : " << svc.address  << "\n"
                          << "    Response: " << svc.response << "\n";
            }
            std::cout << "  " << std::string(60, '-') << "\n";
        }
    }
    std::cout << "  Total stops: " << stops.size() << "\n";
    if (!stops.empty())
    {
        std::cout << "  Source     : " << stops.front().cityName << "\n";
        std::cout << "  Destination: " << stops.back().cityName  << "\n";
        std::cout << "  Total dist : " << stops.back().distFromSource << " km\n";
    }
}
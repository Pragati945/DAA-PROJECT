#include "../include/graph_coloring.h"
#include <iostream>
#include <algorithm>
GraphColoring::GraphColoring(int V) 
{
    color.resize(V, -1);
} 
int GraphColoring::greedyColoring(Graph& g) 
{ 
    std::vector<int> order(g.V);
    for (int i = 0; i < g.V; i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) 
    {
        return g.adj[a].size() > g.adj[b].size();  
    });
    for (int idx = 0; idx < g.V; idx++) 
    {
        int u = order[idx];
        std::vector<bool> takenByNeighbour(g.V,false);
        for (const auto& road : g.adj[u]) 
        {
            if (color[road.destination] != -1)
                takenByNeighbour[color[road.destination]] = true;
        }
        int zone = 0;
        while (zone < g.V && takenByNeighbour[zone]) 
        zone++;
        color[u] = zone;
    }
    int totalZones = *std::max_element(color.begin(), color.end()) + 1;
    std::cout << "\nTraffic Zone Assignment\n";
    std::cout << "  Distinct zones needed: " << totalZones << "\n";
    for (int i = 0; i < g.V; i++)
        std::cout << "  " << g.cities[i].name << "  : Zone " << color[i] << "\n";
    bool valid = true;
    for (int u = 0; u < g.V && valid; u++)
        for (const auto& road : g.adj[u])
            if (color[u] == color[road.destination]) 
            { 
                valid = false; 
                break; 
            }
    std::cout << "  Adjacent cities share no zone: "
              << (valid ? "YES " : "NO") << "\n";
    return totalZones;
}
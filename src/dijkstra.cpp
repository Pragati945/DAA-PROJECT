#include "../include/dijkstra.h"
#include <iostream>
#include <queue>
#include <climits>
#include <stdexcept>
#include <algorithm>
void Dijkstra::printPath(const Graph& g, int dest) const
{
    if (parent[dest] == -1)
    {
        std::cout << g.cities[dest].name;
        return;
    }
    printPath(g, parent[dest]);
    std::cout << " -> " << g.cities[dest].name;
}
void Dijkstra::shortestPath(Graph& g, int source, bool useTraffic)
{
    if (source < 0 || source >= g.V)
        throw std::out_of_range("Source city index is out of range.");
    dist.assign(g.V, INT_MAX);
    parent.assign(g.V, -1);
    using Entry = std::pair<int, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    dist[source] = 0;
    pq.push({0, source});
    while (!pq.empty())
    {
        auto entry = pq.top();
        pq.pop();
        int costSoFar = entry.first;
        int u = entry.second;
        if (costSoFar > dist[u]) continue;
        for (const auto& road : g.adj[u])
        {
            int v = road.destination;
            int weight = useTraffic ? road.effectiveWeight() : road.distance;
            if (dist[u] != INT_MAX && dist[v] > dist[u] + weight)
            {
                dist[v]   = dist[u] + weight;
                parent[v] = u;
                pq.push({dist[v], v});
            }
        }
    }
}
void Dijkstra::printResult(const Graph& g, int source, int dest, bool useTraffic) const
{
    std::string mode = useTraffic
        ? "Traffic-Aware  (cost = distance * traffic)"
        : "Distance Only  (ignores traffic)";
    std::cout << "\n Shortest Path: \""
              << g.cities[source].name << "\"  ->  \""
              << g.cities[dest].name   << "\"  ["
              << mode << "]\n";
    if (dist[dest] == INT_MAX)
    {
        std::cout << "  No route found between these cities.\n";
        return;
    }
    std::cout << "  Total cost : " << dist[dest] << "\n";
    std::cout << "  Route      : ";
    printPath(g, dest);
    std::cout << "\n";
}
std::vector<int> Dijkstra::getRoute(int dest) const
{
    std::vector<int> route;
    if (parent[dest] == -1 && dist[dest] == INT_MAX)
        return route;
    int current = dest;
    while (current != -1)
    {
        route.push_back(current);
        current = parent[current];
    }
    std::reverse(route.begin(), route.end());
    return route;
}
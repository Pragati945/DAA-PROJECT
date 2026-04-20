#ifndef DIJKSTRA_H
#define DIJKSTRA_H
#include "graph.h"
#include <vector>
class Dijkstra
{
public:
    void shortestPath(Graph& g, int source, bool useTraffic = false);
    void printResult(const Graph& g, int source, int dest, bool useTraffic) const;
    std::vector<int> getRoute(int dest) const;
    std::vector<int> getDist() const { return dist; }
private:
    std::vector<int> dist;
    std::vector<int> parent;
    void printPath(const Graph& g, int dest) const;
};
#endif
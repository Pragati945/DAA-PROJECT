#ifndef GRAPH_COLORING_H
#define GRAPH_COLORING_H
#include "graph.h"
#include <vector>
class GraphColoring
{
public:
    std::vector<int> color; 
    explicit GraphColoring(int V);
    int greedyColoring(Graph& g);
};
#endif
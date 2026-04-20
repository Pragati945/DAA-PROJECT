#ifndef EDGE_H
#define EDGE_H
class Edge
{
public:
    int destination;   
    int distance;      
    int traffic;       
    Edge(int destination, int distance, int traffic)
        : destination(destination),
          distance(distance),
          traffic(traffic) {}
    int effectiveWeight() const
    {
        return distance * traffic;
    }
};
#endif
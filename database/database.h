#ifndef DATABASE_H
#define DATABASE_H
#include <string>
#include <vector>
#include <mysql.h>  
#include "graph.h"
#include "graph_coloring.h"
#include "dijkstra.h"
#include "dashboard.h"
class Database
{
public:
    static constexpr const char* HOST = "localhost";
    static constexpr const char* USER = "root";
    static constexpr const char* PASS = "";
    static constexpr const char* DB   = "daa_route_db";
    static constexpr unsigned    PORT = 3306;
    Database();
    ~Database();
    void initSchema();
    void saveCities(const Graph& g);
    void saveRoads(const Graph& g);
    void saveEmergencyServices(const Graph& g);
    bool loadCityNames(std::vector<std::string>& cityNames) const;
    bool loadRoads(Graph& g) const;
    bool loadEmergencyServices(Graph& g) const;
    void saveColoringRun(const GraphColoring& gc, int totalZones, const Graph& g);
    void saveDijkstraResult(int source, int dest, bool useTraffic, int totalCost, const std::vector<int>& path);
    void saveDashboardSession(int source, int dest, bool useTraffic, const std::vector<StopInfo>& stops);
    void printRoadNetwork() const;
    void printLatestDashboard() const;
    void addCity(int id, const std::string& name);
    void addRoad(int srcId, int dstId, int distanceKm, int traffic);
    void updateRoad(int roadId, int newDistance, int newTraffic);
    void deleteRoad(int roadId);
    void upsertEmergencyService(int cityId, const EmergencyService& svc);
    void deleteEmergencyService(int cityId, const std::string& type);
    void printCities() const;
    void printRoadsTable() const;
    void printEmergencyServices(int cityId) const;
private:
    MYSQL* conn_;
    void exec(const std::string& sql);
    unsigned long long lastInsertId() const;
    std::string esc(const std::string& s) const;
};
#endif
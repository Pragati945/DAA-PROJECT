#include "database.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <map>
#include <set>
static MYSQL_RES* safeStore(MYSQL* conn)
{
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res)
        throw std::runtime_error(std::string("mysql_store_result: ") + mysql_error(conn));
    return res;
}
Database::Database() : conn_(nullptr)
{
    conn_ = mysql_init(nullptr);
    if (!conn_)
        throw std::runtime_error("mysql_init() failed — out of memory?");
    if (!mysql_real_connect(conn_, HOST, USER, PASS, DB, PORT, nullptr, 0))
    {
        std::string err = mysql_error(conn_);
        mysql_close(conn_);
        throw std::runtime_error("DB connection failed: " + err);
    }
}
Database::~Database()
{
    if (conn_)
        mysql_close(conn_);
}
void Database::exec(const std::string& sql)
{
    if (mysql_query(conn_, sql.c_str()))
        throw std::runtime_error(std::string("SQL error: ") + mysql_error(conn_));
}
unsigned long long Database::lastInsertId() const
{
    return mysql_insert_id(conn_);
}
std::string Database::esc(const std::string& s) const
{
    std::string buf(s.size() * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(conn_, &buf[0], s.c_str(), (unsigned long)s.size());
    buf.resize(len);
    return buf;
}
void Database::initSchema()
{
    exec(R"(CREATE TABLE IF NOT EXISTS cities (id   INT PRIMARY KEY,name VARCHAR(100) NOT NULL))");
    exec(R"(CREATE TABLE IF NOT EXISTS roads (
            id          INT AUTO_INCREMENT PRIMARY KEY,
            src_id      INT NOT NULL,
            dst_id      INT NOT NULL,
            distance_km INT NOT NULL,
            traffic     INT NOT NULL,
            eff_weight  INT NOT NULL,
            FOREIGN KEY (src_id) REFERENCES cities(id),
            FOREIGN KEY (dst_id) REFERENCES cities(id)
        )
    )");
    exec(R"(CREATE TABLE IF NOT EXISTS emergency_services (
            id        INT AUTO_INCREMENT PRIMARY KEY,
            city_id   INT NOT NULL,
            type      VARCHAR(50)  NOT NULL,
            number    VARCHAR(20)  NOT NULL,
            station   VARCHAR(150) NOT NULL,
            address   VARCHAR(200) NOT NULL,
            response  VARCHAR(100) NOT NULL,
            UNIQUE KEY uq_city_type (city_id, type),
            FOREIGN KEY (city_id) REFERENCES cities(id)
        )
    )");
    exec(R"(CREATE TABLE IF NOT EXISTS dijkstra_results (
            id          INT AUTO_INCREMENT PRIMARY KEY,
            source_id   INT  NOT NULL,
            dest_id     INT  NOT NULL,
            use_traffic BOOL NOT NULL,
            total_cost  INT  NOT NULL,
            path        TEXT NOT NULL,
            created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");
    exec(R"(CREATE TABLE IF NOT EXISTS coloring_runs (
            id          INT AUTO_INCREMENT PRIMARY KEY,
            total_zones INT NOT NULL,
            created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");
    exec(R"(CREATE TABLE IF NOT EXISTS coloring_assignments (
            run_id  INT NOT NULL,
            city_id INT NOT NULL,
            zone    INT NOT NULL,
            PRIMARY KEY (run_id, city_id),
            FOREIGN KEY (run_id)  REFERENCES coloring_runs(id),
            FOREIGN KEY (city_id) REFERENCES cities(id)
        )
    )");
    exec(R"(CREATE TABLE IF NOT EXISTS dashboard_sessions (
            id          INT AUTO_INCREMENT PRIMARY KEY,
            source_id   INT  NOT NULL,
            dest_id     INT  NOT NULL,
            use_traffic BOOL NOT NULL,
            created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");
    exec(R"(CREATE TABLE IF NOT EXISTS dashboard_stops (
            session_id       INT NOT NULL,
            stop_order       INT NOT NULL,
            city_id          INT NOT NULL,
            zone             INT NOT NULL,
            dist_from_source INT NOT NULL,
            next_road_dist   INT,
            next_road_traffic INT,
            next_road_cost   INT,
            PRIMARY KEY (session_id, stop_order),
            FOREIGN KEY (session_id) REFERENCES dashboard_sessions(id),
            FOREIGN KEY (city_id)    REFERENCES cities(id)
        )
    )");
    std::cout << "Schema initialised (tables exist or were just created).\n";
}
void Database::saveCities(const Graph& g)
{
    exec("START TRANSACTION");
    try 
    {
        exec("DELETE FROM cities");
        for (const auto& city : g.cities)
        {
            std::ostringstream q;
            q << "INSERT INTO cities (id, name) VALUES ("<< city.id << ",'" << esc(city.name) << "')";
            exec(q.str());
        }
        exec("COMMIT");
        std::cout << " Cities saved (" << g.cities.size() << " rows).\n";
    }
    catch (...) { exec("ROLLBACK"); throw; }
}
void Database::saveRoads(const Graph& g)
{
    exec("START TRANSACTION");
    try 
    {
        exec("DELETE FROM roads");
        for (int u = 0; u < g.V; u++)
        {
            for (const auto& road : g.adj[u])
            {
                std::ostringstream q;
                q << "INSERT INTO roads (src_id,dst_id,distance_km,traffic,eff_weight) VALUES ("<< u << "," << road.destination << ","
                  << road.distance << "," << road.traffic << ","
                  << road.effectiveWeight() << ")";
                exec(q.str());
            }
        }
        exec("COMMIT");
        std::cout << "Roads saved.\n";
    }
    catch (...) { exec("ROLLBACK"); throw; }
}
void Database::saveEmergencyServices(const Graph& g)
{
    exec("START TRANSACTION");
    try 
    {
        exec("DELETE FROM emergency_services");
        for (const auto& city : g.cities)
        {
            for (const auto& svc : city.services)
            {
                std::ostringstream q;
                q << "INSERT INTO emergency_services " "(city_id,type,number,station,address,response) VALUES ("
                  << city.id << ",'"
                  << esc(svc.type)     << "','"
                  << esc(svc.number)   << "','"
                  << esc(svc.station)  << "','"
                  << esc(svc.address)  << "','"
                  << esc(svc.response) << "')";
                exec(q.str());
            }
        }
        exec("COMMIT");
        std::cout << "Emergency services saved.\n";
    }
    catch (...) { exec("ROLLBACK"); throw; }
}
bool Database::loadCityNames(std::vector<std::string>& cityNames) const
{
    if (mysql_query(conn_, "SELECT id, name FROM cities ORDER BY id"))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW row;
    std::map<int, std::string> names;
    while ((row = mysql_fetch_row(res)))
    {
        if (!row[0] || !row[1])
            continue;
        int id = std::stoi(row[0]);
        if (id < 0)
            throw std::runtime_error("Invalid city id in database.");
        if (names.count(id))
            throw std::runtime_error("Duplicate city id in database.");
        names[id] = row[1];
    }
    mysql_free_result(res);
    if (names.empty())
        return false;
    int maxId = names.rbegin()->first;
    cityNames.assign(maxId + 1, std::string());
    for (auto& entry : names)
    {
        cityNames[entry.first] = entry.second;
    }
    for (int i = 0; i <= maxId; ++i)
    {
        if (cityNames[i].empty())
            throw std::runtime_error("Missing city id " + std::to_string(i) + " in database.");
    }
    return true;
}
bool Database::loadRoads(Graph& g) const
{
    if (g.V == 0)
        return false;
    if (mysql_query(conn_, "SELECT src_id, dst_id, distance_km, traffic FROM roads ORDER BY src_id, dst_id"))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW row;
    std::set<std::pair<int,int>> seen;
    bool any = false;
    while ((row = mysql_fetch_row(res)))
    {
        if (!row[0] || !row[1] || !row[2] || !row[3])
            continue;
        int src = std::stoi(row[0]);
        int dst = std::stoi(row[1]);
        int distance = std::stoi(row[2]);
        int traffic = std::stoi(row[3]);
        if (src < 0 || src >= g.V || dst < 0 || dst >= g.V)
            throw std::runtime_error("Road references invalid city id in database.");
        if (src == dst)
            continue;
        if (seen.count({src, dst}) || seen.count({dst, src}))
            continue;
        g.addEdge(src, dst, distance, traffic);
        seen.insert({src, dst});
        any = true;
    }
    mysql_free_result(res);
    return any;
}
bool Database::loadEmergencyServices(Graph& g) const
{
    if (g.V == 0)
        return false;
    std::ostringstream q;
    q << "SELECT city_id, type, number, station, address, response ""FROM emergency_services ORDER BY city_id, type";
    if (mysql_query(conn_, q.str().c_str()))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW row;
    std::vector<std::vector<EmergencyService>> services(g.V);
    bool any = false;
    while ((row = mysql_fetch_row(res)))
    {
        if (!row[0] || !row[1] || !row[2] || !row[3] || !row[4] || !row[5])
            continue;
        int cityId = std::stoi(row[0]);
        if (cityId < 0 || cityId >= g.V)
            throw std::runtime_error("Emergency service references invalid city id in database.");
        EmergencyService svc;
        svc.type     = row[1];
        svc.number   = row[2];
        svc.station  = row[3];
        svc.address  = row[4];
        svc.response = row[5];
        services[cityId].push_back(svc);
        any = true;
    }
    mysql_free_result(res);
    for (int i = 0; i < g.V; ++i)
        g.addEmergencyServices(i, services[i]);
    return any;
}
void Database::saveColoringRun(const GraphColoring& gc, int totalZones, const Graph& g)
{
    exec("START TRANSACTION");
    try 
    {
        std::ostringstream q;
        q << "INSERT INTO coloring_runs (total_zones) VALUES (" << totalZones << ")";
        exec(q.str());
        unsigned long long runId = lastInsertId();
        for (int i = 0; i < g.V; i++)
        {
            std::ostringstream qa;
            qa << "INSERT INTO coloring_assignments (run_id,city_id,zone) VALUES ("<< runId << "," << i << "," << gc.color[i] << ")";
            exec(qa.str());
        }
        exec("COMMIT");
        std::cout << "Coloring run saved (run_id=" << runId << ", zones=" << totalZones << ").\n";
    }
    catch (...) { exec("ROLLBACK"); throw; }
}
void Database::saveDijkstraResult(int source, int dest, bool useTraffic,int totalCost, const std::vector<int>& path)
{
    std::string pathStr;
    for (size_t i = 0; i < path.size(); i++)
    {
        std::ostringstream q;
        q << "SELECT name FROM cities WHERE id=" << path[i];
        if (mysql_query(conn_, q.str().c_str()))
            throw std::runtime_error(mysql_error(conn_));
        MYSQL_RES* res = safeStore(conn_);
        MYSQL_ROW  row = mysql_fetch_row(res);
        if (row && row[0]) pathStr += row[0];
        mysql_free_result(res);
        if (i + 1 < path.size()) pathStr += " -> ";
    }
    std::ostringstream q;
    q << "INSERT INTO dijkstra_results (source_id,dest_id,use_traffic,total_cost,path) VALUES ("
      << source << "," << dest << "," << (useTraffic ? 1 : 0) << ","
      << totalCost << ",'" << esc(pathStr) << "')";
    exec(q.str());
    std::cout << "Dijkstra result saved.\n";
}
void Database::saveDashboardSession(int source, int dest, bool useTraffic, const std::vector<StopInfo>& stops)
{
    exec("START TRANSACTION");
    try 
    {
        std::ostringstream q;
        q << "INSERT INTO dashboard_sessions (source_id,dest_id,use_traffic) VALUES ("<< source << "," << dest << "," << (useTraffic ? 1 : 0) << ")";
        exec(q.str());
        unsigned long long sessionId = lastInsertId();
        for (int i = 0; i < (int)stops.size(); i++)
        {
            const auto& s = stops[i];
            std::ostringstream qs;
            qs << "INSERT INTO dashboard_stops ""(session_id,stop_order,city_id,zone,dist_from_source,""next_road_dist,next_road_traffic,next_road_cost) VALUES ("
               << sessionId << "," << i << "," << s.cityId << "," << s.zone
               << "," << s.distFromSource << ","
               << s.nextRoadDist << "," << s.nextRoadTraffic << "," << s.nextRoadCost
               << ")";
            exec(qs.str());
        }
        exec("COMMIT");
        std::cout << "Dashboard session saved (id=" << sessionId << ").\n";
    }
    catch (...) { exec("ROLLBACK"); throw; }
}
void Database::printRoadNetwork() const
{
    const char* sql =
        "SELECT c1.name, c2.name, r.distance_km, r.traffic, r.eff_weight "
        "FROM roads r "
        "JOIN cities c1 ON c1.id = r.src_id "
        "JOIN cities c2 ON c2.id = r.dst_id "
        "ORDER BY c1.name, c2.name";
    if (mysql_query(conn_, sql))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW  row;
    std::cout << "\n--- Road Network (from DB) ---\n";
    std::cout << std::left<< std::setw(18) << "From"<< std::setw(18) << "To"<< std::setw(10) << "Dist(km)"<< std::setw(10) << "Traffic"
              << std::setw(10) << "Cost" << "\n";
    std::cout << std::string(66, '-') << "\n";
    while ((row = mysql_fetch_row(res)))
    {
        if (!row[0] || !row[1]) continue;
        std::cout << std::left
                  << std::setw(18) << row[0]
                  << std::setw(18) << row[1]
                  << std::setw(10) << row[2]
                  << std::setw(10) << row[3]
                  << std::setw(10) << row[4] << "\n";
    }
    mysql_free_result(res);
}
void Database::printLatestDashboard() const
{
    if (mysql_query(conn_, "SELECT MAX(id) FROM dashboard_sessions"))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW  row = mysql_fetch_row(res);
    if (!row || !row[0]) { mysql_free_result(res); std::cout << "No dashboard session found.\n"; return; }
    int sessionId = std::stoi(row[0]);
    mysql_free_result(res);
    std::ostringstream q;
    q << "SELECT ds.stop_order, c.name, ds.zone, ds.dist_from_source, "
         "ds.next_road_dist, ds.next_road_traffic, ds.next_road_cost "
         "FROM dashboard_stops ds "
         "JOIN cities c ON c.id = ds.city_id "
         "WHERE ds.session_id=" << sessionId << " ORDER BY ds.stop_order";
    if (mysql_query(conn_, q.str().c_str()))
        throw std::runtime_error(mysql_error(conn_));
    res = safeStore(conn_);
    std::cout << "\n Latest Dashboard Session (id=" << sessionId << ") \n";
    while ((row = mysql_fetch_row(res)))
    {
        std::cout << "Stop " << row[0] << ": " << row[1]<< "  Zone=" << row[2]<< "  Dist=" << row[3] << " km";
        if (row[4] && std::string(row[4]) != "-1")
            std::cout << "  NextRoad=" << row[4] << "km"<< " Traffic=" << row[5]<< " Cost=" << row[6];
        std::cout << "\n";
    }
    mysql_free_result(res);
}
void Database::addCity(int id, const std::string& name)
{
    std::ostringstream q;
    q << "INSERT INTO cities (id, name) VALUES (" << id << ",'" << esc(name) << "')";
    exec(q.str());
    std::cout << "City added: " << id << " " << name << "\n";
}
void Database::addRoad(int srcId, int dstId, int distanceKm, int traffic)
{
    int eff = distanceKm * traffic;
    exec("START TRANSACTION");
    try 
    {
        std::ostringstream q1, q2;
        q1 << "INSERT INTO roads (src_id,dst_id,distance_km,traffic,eff_weight) VALUES ("
           << srcId << "," << dstId << "," << distanceKm << "," << traffic << "," << eff << ")";
        q2 << "INSERT INTO roads (src_id,dst_id,distance_km,traffic,eff_weight) VALUES ("
           << dstId << "," << srcId << "," << distanceKm << "," << traffic << "," << eff << ")";
        exec(q1.str());
        exec(q2.str());
        exec("COMMIT");
        std::cout << "Road added (both directions).\n";
    }
    catch (...) { exec("ROLLBACK"); throw; }
}
void Database::updateRoad(int roadId, int newDistance, int newTraffic)
{
    int eff = newDistance * newTraffic;
    std::ostringstream q;
    q << "UPDATE roads SET distance_km=" << newDistance<< ", traffic=" << newTraffic<< ", eff_weight=" << eff<< " WHERE id=" << roadId;
    exec(q.str());
    std::cout << " Road id=" << roadId << " updated.\n";
}
void Database::deleteRoad(int roadId)
{
    std::ostringstream q;
    q << "DELETE FROM roads WHERE id=" << roadId;
    exec(q.str());
    std::cout << " Road id=" << roadId << " deleted.\n";
}
void Database::upsertEmergencyService(int cityId, const EmergencyService& svc)
{
    std::ostringstream q;
    q << "INSERT INTO emergency_services (city_id,type,number,station,address,response) VALUES ("
      << cityId << ",'"
      << esc(svc.type)     << "','"
      << esc(svc.number)   << "','"
      << esc(svc.station)  << "','"
      << esc(svc.address)  << "','"
      << esc(svc.response) << "')"
      << " ON DUPLICATE KEY UPDATE "
      << "number='"   << esc(svc.number)   << "',"
      << "station='"  << esc(svc.station)  << "',"
      << "address='"  << esc(svc.address)  << "',"
      << "response='" << esc(svc.response) << "'";
    exec(q.str());
    std::cout << "[DB] Emergency service upserted for city " << cityId << ".\n";
}
void Database::deleteEmergencyService(int cityId, const std::string& type)
{
    std::ostringstream q;
    q << "DELETE FROM emergency_services WHERE city_id=" << cityId<< " AND type='" << esc(type) << "'";
    exec(q.str());
    std::cout << "[DB] Emergency service '" << type << "' deleted for city " << cityId << ".\n";
}
void Database::printCities() const
{
    if (mysql_query(conn_, "SELECT id, name FROM cities ORDER BY id"))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW  row;
    std::cout << "\n--- Cities ---\n";
    while ((row = mysql_fetch_row(res)))
    {
        if (!row[0] || !row[1]) continue;
        std::cout << "  [" << row[0] << "] " << row[1] << "\n";
    }
    mysql_free_result(res);
}
void Database::printRoadsTable() const
{
    const char* sql =
        "SELECT r.id, c1.name, c2.name, r.distance_km, r.traffic, r.eff_weight "
        "FROM roads r "
        "JOIN cities c1 ON c1.id = r.src_id "
        "JOIN cities c2 ON c2.id = r.dst_id "
        "ORDER BY r.id";
    if (mysql_query(conn_, sql))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW  row;
    std::cout << "\n--- Roads Table ---\n";
    std::cout << std::left
              << std::setw(6)  << "RowID"
              << std::setw(18) << "From"
              << std::setw(18) << "To"
              << std::setw(10) << "Dist(km)"
              << std::setw(10) << "Traffic"
              << std::setw(10) << "Cost" << "\n";
    std::cout << std::string(72, '-') << "\n";
    while ((row = mysql_fetch_row(res)))
    {
        if (!row[0]) continue;
        std::cout << std::left
                  << std::setw(6)  << row[0]
                  << std::setw(18) << (row[1] ? row[1] : "?")
                  << std::setw(18) << (row[2] ? row[2] : "?")
                  << std::setw(10) << (row[3] ? row[3] : "?")
                  << std::setw(10) << (row[4] ? row[4] : "?")
                  << std::setw(10) << (row[5] ? row[5] : "?") << "\n";
    }
    mysql_free_result(res);
}
void Database::printEmergencyServices(int cityId) const
{
    std::ostringstream q;
    q << "SELECT type,number,station,address,response ""FROM emergency_services WHERE city_id=" << cityId<< " ORDER BY type";
    if (mysql_query(conn_, q.str().c_str()))
        throw std::runtime_error(mysql_error(conn_));
    MYSQL_RES* res = safeStore(conn_);
    MYSQL_ROW  row;
    std::cout << "\n Emergency Services for city id=" << cityId << "\n";
    bool any = false;
    while ((row = mysql_fetch_row(res)))
    {
        any = true;
        std::cout << "  [" << (row[0] ? row[0] : "?") << "]\n"
                  << "    Number  : " << (row[1] ? row[1] : "?") << "\n"
                  << "    Station : " << (row[2] ? row[2] : "?") << "\n"
                  << "    Address : " << (row[3] ? row[3] : "?") << "\n"
                  << "    Response: " << (row[4] ? row[4] : "?") << "\n";
    }
    if (!any) std::cout << "  (none found)\n";
    mysql_free_result(res);
}
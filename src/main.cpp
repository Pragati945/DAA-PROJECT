#include <iostream>
#include <memory>
#include <stdexcept>
#include "../include/graph.h"
#include "../include/graph_coloring.h"
#include "../include/dijkstra.h"
#include "../include/dashboard.h"
#include "../database/database.h"
//http://localhost/daa pbl/index.html
using namespace std;
unique_ptr<Graph> loadGraphFromDB(Database& db, vector<string>& cityNames)
{
    cityNames.clear();
    if (!db.loadCityNames(cityNames))
        throw runtime_error("No graph data found in database. Please populate the database first.");
    auto g = make_unique<Graph>(static_cast<int>(cityNames.size()), cityNames);
    if (!db.loadRoads(*g))
        throw runtime_error("Database contains cities but no roads.");
    if (!db.loadEmergencyServices(*g))
        cout << " No emergency services found in database.\n";
    cout << " Graph loaded from database.\n";
    return g;
}
int main()
{
    try
    {
        Database db;
        db.initSchema();
        cout << "\nDATABASE SETUP / UPDATE\n";
        cout << "Connected. Update your data before analysis starts.\n\n";
        bool running = true;
        while (running)
        {
            cout <<   "DATABASE UPDATE MENU\n";
            cout <<   "1. Add a new city\n";
            cout <<   "2. Add a new road (both directions)\n";
            cout <<   "3. Update an existing road\n";
            cout <<   "4. Delete a road by row-id\n";
            cout <<   "5. Add/update an emergency service\n";
            cout <<   "6. Delete an emergency service\n";
            cout <<   "7. Show all cities\n";
            cout <<   "8. Show all roads (with row-ids)\n";
            cout <<   "9. Show emergency services \n";
            cout <<   "0. Done proceed to analysis \n";
            cout << "Choice: ";
            int choice;
            cin >> choice;
            if (choice == 0)
            {
                running = false;
            }
            else if (choice == 1)
            {
                int id;
                string name;
                cout << "New city ID: ";
                cin >> id;
                cout << "City name: ";
                cin.ignore();
                getline(cin, name);
                db.addCity(id, name);
            }
            else if (choice == 2)
            {
                cout << "Available cities:\n";
                db.printCities();
                int src, dst, dist, traf;
                cout << "Source city ID      : "; cin >> src;
                cout << "Destination city ID : "; cin >> dst;
                cout << "Distance (km)       : "; cin >> dist;
                cout << "Traffic level (1-5) : "; cin >> traf;
                db.addRoad(src, dst, dist, traf);
            }
            else if (choice == 3)
            {
                cout << "Current roads:\n";
                db.printRoadsTable();
                int rid, dist, traf;
                cout << "Road row-id to edit : "; cin >> rid;
                cout << "New distance (km)   : "; cin >> dist;
                cout << "New traffic (1-5)   : "; cin >> traf;
                db.updateRoad(rid, dist, traf);
            }
            else if (choice == 4)
            {
                cout << "Current roads:\n";
                db.printRoadsTable();
                int rid;
                cout << "Road row-id to delete: "; cin >> rid;
                char confirm;
                cout << "Are you sure (y/n): "; cin >> confirm;
                if (confirm == 'y' || confirm == 'Y')
                    db.deleteRoad(rid);
                else
                    cout << "Cancelled.\n";
            }
            else if (choice == 5)
            {
                cout << "Available cities:\n";
                db.printCities();
                int cid;
                cout << "City ID: "; cin >> cid;
                db.printEmergencyServices(cid);
                EmergencyService svc;
                cin.ignore();
                cout << "Service type (Police/Ambulance/Fire/Highway): ";
                getline(cin, svc.type);
                cout << "Contact number : "; getline(cin, svc.number);
                cout << "Station name   : "; getline(cin, svc.station);
                cout << "Address        : "; getline(cin, svc.address);
                cout << "Response time  : "; getline(cin, svc.response);
                db.upsertEmergencyService(cid, svc);
            }
            else if (choice == 6)
            {
                cout << "Available cities:\n";
                db.printCities();
                int cid;
                cout << "City ID: "; cin >> cid;
                db.printEmergencyServices(cid);
                string type;
                cin.ignore();
                cout << "Service type to delete: "; getline(cin, type);
                char confirm;
                cout << "Are you sure (y/n): "; cin >> confirm;
                if (confirm == 'y' || confirm == 'Y')
                    db.deleteEmergencyService(cid, type);
                else
                    cout << "Cancelled.\n";
            }
            else if (choice == 7)
            {
                db.printCities();
            }
            else if (choice == 8)
            {
                db.printRoadsTable();
            }
            else if (choice == 9)
            {
                cout << "Available cities:\n";
                db.printCities();
                int cid;
                cout << "City ID: "; cin >> cid;
                db.printEmergencyServices(cid);
            }
            else
            {
                cout << "Invalid choice please enter 0-9.\n";
            }
        }
        cout << "\nLoading graph from updated database...\n";
        vector<string> cityNames;
        unique_ptr<Graph> g = loadGraphFromDB(db, cityNames);
        int V = g->V;
        cout << "\n ROAD NETWORK \n";
        g->printGraph();
        cout << "\n ZONE COLORING \n";
        GraphColoring gc(V);
        int totalZones = gc.greedyColoring(*g);
        cout << "Total zones assigned: " << totalZones << "\n";
        cout << "\n SHORTEST PATH ANALYSIS \n";
        cout << "Available cities:\n";
        for (int i = 0; i < V; i++)
            cout << "  " << i << ": " << cityNames[i] << "\n";
        int sourceCity;
        cout << "\nEnter source city index: ";
        cin >> sourceCity;
        if (sourceCity < 0 || sourceCity >= V)
            throw runtime_error("Invalid source city index.");

        cout << "\n TRAVEL DASHBOARD \n";
        cout << "Choose destination city:\n";
        for (int i = 0; i < V; i++)
            if (i != sourceCity)
                cout << "  " << i << ": " << cityNames[i] << "\n";
        int destCity;
        cout << "Enter destination city index: ";
        cin >> destCity;
        if (destCity < 0 || destCity >= V || destCity == sourceCity)
            throw runtime_error("Invalid destination city index.");

        // Show both route options BEFORE asking which to use
        Dijkstra byDistance;
        byDistance.shortestPath(*g, sourceCity, false);
        byDistance.printResult(*g, sourceCity, destCity, false);

        Dijkstra byTraffic;
        byTraffic.shortestPath(*g, sourceCity, true);
        byTraffic.printResult(*g, sourceCity, destCity, true);

        char useTraffic;
        cout << "\nUse traffic-aware routing? (y/n): ";
        cin >> useTraffic;
        bool trafficFlag = (useTraffic == 'y' || useTraffic == 'Y');

        Dijkstra dashDijkstra;
        GraphColoring dashGC(V);
        Dashboard dash;
        cout << "\nBuilding travel dashboard: " << cityNames[sourceCity]
             << " -> " << cityNames[destCity]
             << (trafficFlag ? " (traffic-aware)" : " (distance-only)") << "\n";
        auto stops = dash.buildRoute(*g, dashDijkstra, dashGC,
                                     sourceCity, destCity, trafficFlag);
        dash.printDashboard(stops);
        cout << "\n Session ended. \n";
    }
    catch (const exception& e)
    {
        cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
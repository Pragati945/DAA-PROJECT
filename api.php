<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(200); exit; }

define('DB_HOST', 'localhost');
define('DB_USER', 'root');
define('DB_PASS', '');
define('DB_NAME', 'daa_route_db');
define('DB_PORT', 3306);

function getDB() {
    $conn = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT);
    if ($conn->connect_error) {
        http_response_code(500);
        echo json_encode(['error' => 'DB connection failed: ' . $conn->connect_error]);
        exit;
    }
    return $conn;
}

function respond($data) { echo json_encode($data); exit; }
function err($msg, $code = 400) { http_response_code($code); echo json_encode(['error' => $msg]); exit; }

$action = $_GET['action'] ?? '';
$db = getDB();

switch ($action) {

    // ── Cities ────────────────────────────────────────────────
    case 'get_cities':
        $res = $db->query("SELECT id, name FROM cities ORDER BY id");
        $rows = [];
        while ($r = $res->fetch_assoc()) $rows[] = $r;
        respond($rows);

    case 'add_city':
        $body = json_decode(file_get_contents('php://input'), true);
        $id   = (int)$body['id'];
        $name = $db->real_escape_string($body['name']);
        if (!$id || !$name) err('id and name required');
        if (!$db->query("INSERT INTO cities (id, name) VALUES ($id, '$name')"))
            err($db->error);
        respond(['success' => true, 'message' => "City '$name' added."]);

    case 'delete_city':
        $id = (int)($_GET['id'] ?? 0);
        if (!$id) err('id required');
        $db->query("DELETE FROM roads WHERE src_id=$id OR dst_id=$id");
        $db->query("DELETE FROM emergency_services WHERE city_id=$id");
        if (!$db->query("DELETE FROM cities WHERE id=$id")) err($db->error);
        respond(['success' => true]);

    // ── Roads ─────────────────────────────────────────────────
    case 'get_roads':
        $res = $db->query(
            "SELECT r.id, c1.name AS src, c2.name AS dst, r.src_id, r.dst_id,
                    r.distance_km, r.traffic, r.eff_weight
             FROM roads r
             JOIN cities c1 ON c1.id = r.src_id
             JOIN cities c2 ON c2.id = r.dst_id
             ORDER BY r.id"
        );
        $rows = [];
        while ($r = $res->fetch_assoc()) $rows[] = $r;
        respond($rows);

    case 'add_road':
        $body    = json_decode(file_get_contents('php://input'), true);
        $src     = (int)$body['src_id'];
        $dst     = (int)$body['dst_id'];
        $dist    = (int)$body['distance_km'];
        $traffic = (int)$body['traffic'];
        if (!$src || !$dst || !$dist || !$traffic) err('All fields required');
        if ($src === $dst) err('Source and destination must differ');
        $eff = $dist * $traffic;
        $db->begin_transaction();
        $ok  = $db->query("INSERT INTO roads (src_id,dst_id,distance_km,traffic,eff_weight) VALUES ($src,$dst,$dist,$traffic,$eff)");
        $ok2 = $db->query("INSERT INTO roads (src_id,dst_id,distance_km,traffic,eff_weight) VALUES ($dst,$src,$dist,$traffic,$eff)");
        if ($ok && $ok2) { $db->commit(); respond(['success' => true, 'message' => 'Road added (both directions).']); }
        else { $db->rollback(); err($db->error); }

    case 'update_road':
        $body    = json_decode(file_get_contents('php://input'), true);
        $id      = (int)$body['id'];
        $dist    = (int)$body['distance_km'];
        $traffic = (int)$body['traffic'];
        if (!$id || !$dist || !$traffic) err('All fields required');
        $eff = $dist * $traffic;
        if (!$db->query("UPDATE roads SET distance_km=$dist, traffic=$traffic, eff_weight=$eff WHERE id=$id"))
            err($db->error);
        respond(['success' => true, 'message' => "Road #$id updated."]);

    case 'delete_road':
        $id = (int)($_GET['id'] ?? 0);
        if (!$id) err('id required');
        if (!$db->query("DELETE FROM roads WHERE id=$id")) err($db->error);
        respond(['success' => true]);

    // ── Emergency Services ────────────────────────────────────
    case 'get_emergency':
        $city_id = (int)($_GET['city_id'] ?? 0);
        $where   = $city_id ? "WHERE city_id=$city_id" : '';
        $res     = $db->query(
            "SELECT es.*, c.name AS city_name
             FROM emergency_services es
             JOIN cities c ON c.id = es.city_id
             $where ORDER BY city_id, type"
        );
        $rows = [];
        while ($r = $res->fetch_assoc()) $rows[] = $r;
        respond($rows);

    case 'upsert_emergency':
        $body     = json_decode(file_get_contents('php://input'), true);
        $city_id  = (int)$body['city_id'];
        $type     = $db->real_escape_string($body['type']);
        $number   = $db->real_escape_string($body['number']);
        $station  = $db->real_escape_string($body['station']);
        $address  = $db->real_escape_string($body['address']);
        $response = $db->real_escape_string($body['response']);
        if (!$city_id || !$type) err('city_id and type required');
        $sql = "INSERT INTO emergency_services (city_id,type,number,station,address,response)
                VALUES ($city_id,'$type','$number','$station','$address','$response')
                ON DUPLICATE KEY UPDATE
                  number='$number', station='$station', address='$address', response='$response'";
        if (!$db->query($sql)) err($db->error);
        respond(['success' => true, 'message' => 'Emergency service saved.']);

    case 'delete_emergency':
        $city_id = (int)($_GET['city_id'] ?? 0);
        $type    = $db->real_escape_string($_GET['type'] ?? '');
        if (!$city_id || !$type) err('city_id and type required');
        if (!$db->query("DELETE FROM emergency_services WHERE city_id=$city_id AND type='$type'"))
            err($db->error);
        respond(['success' => true]);

    // ── Dijkstra (PHP implementation mirrors C++ logic) ───────
    case 'shortest_path':
        $src        = (int)($_GET['src'] ?? -1);
        $dst        = (int)($_GET['dst'] ?? -1);
        $use_traffic = ($_GET['use_traffic'] ?? '0') === '1';

        // Load cities
        $res = $db->query("SELECT id, name FROM cities ORDER BY id");
        $cities = [];
        while ($r = $res->fetch_assoc()) $cities[$r['id']] = $r['name'];
        if (!isset($cities[$src]) || !isset($cities[$dst])) err('Invalid city ids');

        // Load adjacency list
        $res = $db->query("SELECT src_id, dst_id, distance_km, traffic, eff_weight FROM roads");
        $adj = [];
        foreach ($cities as $id => $_) $adj[$id] = [];
        while ($r = $res->fetch_assoc()) {
            $adj[$r['src_id']][] = [
                'dst'     => (int)$r['dst_id'],
                'dist'    => (int)$r['distance_km'],
                'traffic' => (int)$r['traffic'],
                'eff'     => (int)$r['eff_weight'],
            ];
        }

        // Dijkstra
        $INF    = PHP_INT_MAX;
        $dist   = array_fill_keys(array_keys($cities), $INF);
        $parent = array_fill_keys(array_keys($cities), -1);
        $dist[$src] = 0;
        $visited    = [];
        $pq         = new SplMinHeap();
        $pq->insert([0, $src]);

        while (!$pq->isEmpty()) {
            [$cost, $u] = $pq->extract();
            if (isset($visited[$u])) continue;
            $visited[$u] = true;
            foreach ($adj[$u] as $edge) {
                $v      = $edge['dst'];
                $weight = $use_traffic ? $edge['eff'] : $edge['dist'];
                if ($dist[$u] !== $INF && $dist[$u] + $weight < $dist[$v]) {
                    $dist[$v]   = $dist[$u] + $weight;
                    $parent[$v] = $u;
                    $pq->insert([$dist[$v], $v]);
                }
            }
        }

        // Reconstruct path
        $path = [];
        $cur  = $dst;
        while ($cur !== -1) { array_unshift($path, $cur); $cur = $parent[$cur]; }

        // Build stop details for dashboard
        $stops = [];
        for ($i = 0; $i < count($path); $i++) {
            $u = $path[$i];
            // Emergency services for this city
            $svc_res = $db->query("SELECT * FROM emergency_services WHERE city_id=$u ORDER BY type");
            $svcs = [];
            while ($s = $svc_res->fetch_assoc()) $svcs[] = $s;

            // Zone from coloring (greedy, computed server-side below)
            $stop = [
                'city_id'       => $u,
                'city_name'     => $cities[$u],
                'dist_from_src' => $dist[$u],
                'services'      => $svcs,
                'next_dist'     => null,
                'next_traffic'  => null,
                'next_cost'     => null,
            ];
            if ($i + 1 < count($path)) {
                $next = $path[$i + 1];
                foreach ($adj[$u] as $edge) {
                    if ($edge['dst'] === $next) {
                        $stop['next_dist']    = $edge['dist'];
                        $stop['next_traffic'] = $edge['traffic'];
                        $stop['next_cost']    = $edge['eff'];
                        break;
                    }
                }
            }
            $stops[] = $stop;
        }

        // Greedy graph coloring
        $degrees = array_fill_keys(array_keys($cities), 0);
        foreach ($adj as $u => $edges) $degrees[$u] = count($edges);
        arsort($degrees);
        $order  = array_keys($degrees);
        $colors = array_fill_keys(array_keys($cities), -1);
        foreach ($order as $u) {
            $taken = [];
            foreach ($adj[$u] as $edge) {
                if ($colors[$edge['dst']] !== -1) $taken[$colors[$edge['dst']]] = true;
            }
            $zone = 0;
            while (isset($taken[$zone])) $zone++;
            $colors[$u] = $zone;
        }
        // Attach zone to each stop
        foreach ($stops as &$stop) $stop['zone'] = $colors[$stop['city_id']];
        unset($stop);

        // All cities with zones (for network graph display)
        $all_zones = [];
        foreach ($cities as $id => $name) $all_zones[] = ['id' => $id, 'name' => $name, 'zone' => $colors[$id]];

        // All roads for network display
        $roads_res = $db->query("SELECT src_id, dst_id, distance_km, traffic, eff_weight FROM roads");
        $all_roads = [];
        while ($r = $roads_res->fetch_assoc()) $all_roads[] = $r;

        respond([
            'src_name'    => $cities[$src],
            'dst_name'    => $cities[$dst],
            'use_traffic' => $use_traffic,
            'total_cost'  => $dist[$dst] === PHP_INT_MAX ? null : $dist[$dst],
            'path_ids'    => $path,
            'path_names'  => array_map(fn($id) => $cities[$id], $path),
            'stops'       => $stops,
            'all_zones'   => $all_zones,
            'all_roads'   => $all_roads,
            'total_zones' => max(array_values($colors)) + 1,
        ]);

    // ── Road network summary ──────────────────────────────────
    case 'get_network':
        $cities_res = $db->query("SELECT id, name FROM cities ORDER BY id");
        $cities = [];
        while ($r = $cities_res->fetch_assoc()) $cities[] = $r;

        $roads_res = $db->query(
            "SELECT r.id, r.src_id, r.dst_id, r.distance_km, r.traffic, r.eff_weight,
                    c1.name AS src_name, c2.name AS dst_name
             FROM roads r
             JOIN cities c1 ON c1.id=r.src_id
             JOIN cities c2 ON c2.id=r.dst_id ORDER BY r.id"
        );
        $roads = [];
        while ($r = $roads_res->fetch_assoc()) $roads[] = $r;
        respond(['cities' => $cities, 'roads' => $roads]);

    default:
        err("Unknown action: $action", 404);
}
$db->close();

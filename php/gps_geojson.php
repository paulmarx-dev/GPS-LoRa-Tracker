<?php
// gps_geojson.php
// Outputs GeoJSON FeatureCollection (LineString + optional Points) from rotated daily CSV files.
//
// Query params:
//   device=default
//   begin_ts=1700000000   (optional; epoch seconds, inclusive)
//   end_ts=1700086400     (optional; epoch seconds, inclusive)
//   hours=24              (optional fallback if begin/end not given; capped at 168)
//   points=1              (optional; include Point features; default 0)
//
// Auth:
//   X-API-Token header (same token as gps_batch.php)
//   X-Device-Id header optional (device can also be passed via ?device=)
//
// CSV columns expected:
//   seq,ts_iso,ts_epoch,latE7,lonE7,lat,lon,ch,net,bat,flags
//
// GeoJSON coordinates are [lon, lat]

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('Pragma: no-cache');
header('Expires: 0');

// ================== HELPERS ==================
/**
 * Calculate distance between two lat/lon points using Haversine formula
 * Returns distance in meters
 */
function haversineDistance(float $lat1, float $lon1, float $lat2, float $lon2): float {
  $R = 6371000; // Earth's radius in meters
  $dLat = deg2rad($lat2 - $lat1);
  $dLon = deg2rad($lon2 - $lon1);
  $a = sin($dLat / 2) * sin($dLat / 2) +
       cos(deg2rad($lat1)) * cos(deg2rad($lat2)) *
       sin($dLon / 2) * sin($dLon / 2);
  $c = 2 * atan2(sqrt($a), sqrt(1 - $a));
  return $R * $c;
}

// ================== CONFIG ==================
$BASE_DIR = __DIR__ . '/data';
$EXPECTED_TOKEN = 'CHANGE_ME_LONG_RANDOM_TOKEN';
$MAX_HOURS = 168; // 7 days

// ================== AUTH ==================
$token = $_SERVER['HTTP_X_API_TOKEN'] ?? '';
if (!hash_equals($EXPECTED_TOKEN, $token)) {
  http_response_code(401);
  echo json_encode(['ok' => false, 'error' => 'unauthorized']);
  exit;
}

// ================== INPUT ==================
$deviceRaw = $_GET['device'] ?? ($_SERVER['HTTP_X_DEVICE_ID'] ?? 'default');
$DEVICE_ID = preg_replace('/[^a-zA-Z0-9_\-]/', '', (string)$deviceRaw);
if ($DEVICE_ID === '') $DEVICE_ID = 'default';

$includePoints = (int)($_GET['points'] ?? 0) === 1;

$now = time();

// Time window resolution:
// 1) if begin_ts and end_ts provided -> use them
// 2) else fallback to hours=N anchored at now
$beginTsParam = $_GET['begin_ts'] ?? null;
$endTsParam   = $_GET['end_ts'] ?? null;

$useAbsoluteWindow = false;
$fromTs = 0;
$toTs = 0;

if (
  $beginTsParam !== null && $endTsParam !== null &&
  ctype_digit((string)$beginTsParam) && ctype_digit((string)$endTsParam)
) {
  $fromTs = (int)$beginTsParam;
  $toTs   = (int)$endTsParam;
  $useAbsoluteWindow = true;
} else {
  $hours = (int)($_GET['hours'] ?? 24);
  if ($hours <= 0) $hours = 24;
  if ($hours > $MAX_HOURS) $hours = $MAX_HOURS;

  $toTs = $now + 2 * 3600; // allow small future skew to account for clock differences and in-flight data
  $fromTs = $now - ($hours * 3600);
}

// sanitize window
if ($fromTs < 0) $fromTs = 0;
if ($toTs < 0) $toTs = 0;

// Clamp end to now
if ($toTs > $now) $toTs = $now;

// Ensure order
if ($toTs <= $fromTs) {
  http_response_code(400);
  echo json_encode(['ok' => false, 'error' => 'invalid time window (end must be > begin)']);
  exit;
}

// Enforce max window length (7 days)
if (($toTs - $fromTs) > ($MAX_HOURS * 3600)) {
  $fromTs = $toTs - ($MAX_HOURS * 3600);
}

$devDir = $BASE_DIR . '/' . $DEVICE_ID;
if (!is_dir($devDir)) {
  http_response_code(404);
  echo json_encode(['ok' => false, 'error' => 'device not found']);
  exit;
}

// Determine which daily files to read (UTC dates)
$startDate = gmdate('Y-m-d', $fromTs);
$endDate   = gmdate('Y-m-d', $toTs);

function dateRangeUtc(string $start, string $end): array {
  $out = [];
  $t = strtotime($start . ' 00:00:00 UTC');
  $e = strtotime($end   . ' 00:00:00 UTC');
  if ($t === false || $e === false) return $out;
  for ($cur = $t; $cur <= $e; $cur += 86400) {
    $out[] = gmdate('Y-m-d', $cur);
  }
  return $out;
}

$dates = dateRangeUtc($startDate, $endDate);

// ================== READ CSVs ==================
$coords = [];   // LineString coordinates [[lon,lat], ...]
$points = [];   // optional point features
$prevLat = null;
$prevLon = null;
$prevTs = null;

foreach ($dates as $d) {
  $file = $devDir . '/' . $d . '.csv';
  if (!file_exists($file)) continue;

  $fp = fopen($file, 'rb');
  if ($fp === false) continue;

  // Skip header
  fgets($fp);

  while (($line = fgets($fp)) !== false) {
    $line = trim($line);
    if ($line === '') continue;

    $cols = str_getcsv($line);
    if (count($cols) < 8) continue;

    $seq = (int)$cols[0];
    $ts  = (int)$cols[2];
    $lat = (float)$cols[5];
    $lon = (float)$cols[6];
    $ch  = (string)$cols[7];
    $net = $cols[8] ?? '';
    $bat = null;
    $flags = null;
    if (isset($cols[9]) && $cols[9] !== '') $bat = (int)$cols[9];
    if (isset($cols[10]) && $cols[10] !== '') $flags = (int)$cols[10];

    // time window
    if ($ts < $fromTs || $ts > $toTs) continue;

    // sanity
    if ($lat < -90 || $lat > 90 || $lon < -180 || $lon > 180) continue;

    $coords[] = [$lon, $lat];

    if ($includePoints) {
      $props = [
        'seq' => $seq,
        'ts' => $ts,
        'ch' => $ch,
        'net' => $net,
      ];
      if ($bat !== null) $props['bat'] = $bat;
      if ($flags !== null) $props['flags'] = $flags;
      
      // Calculate speed from previous point
      if ($prevLat !== null && $prevLon !== null && $prevTs !== null) {
        $timeDelta = $ts - $prevTs;
        if ($timeDelta > 0) {
          $distance = haversineDistance($prevLat, $prevLon, $lat, $lon);
          $speedMps = $distance / $timeDelta;
          $speedKmh = $speedMps * 3.6;
          $props['speed_mps'] = round($speedMps, 2);
          $props['speed_kmh'] = round($speedKmh, 2);
        }
      }

      $points[] = [
        'type' => 'Feature',
        'geometry' => [
          'type' => 'Point',
          'coordinates' => [$lon, $lat],
        ],
        'properties' => $props,
      ];
    }
    
    // Update previous point for next iteration
    $prevLat = $lat;
    $prevLon = $lon;
    $prevTs = $ts;
  }

  fclose($fp);
}

// ================== BUILD GEOJSON ==================
$features = [];

if (count($coords) >= 2) {
  $features[] = [
    'type' => 'Feature',
    'geometry' => [
      'type' => 'LineString',
      'coordinates' => $coords,
    ],
    'properties' => [
      'device' => $DEVICE_ID,
      'from_ts' => $fromTs,
      'to_ts' => $toTs,
      'points' => count($coords),
      'mode' => $useAbsoluteWindow ? 'absolute' : 'hours',
    ],
  ];
} elseif (count($coords) === 1) {
  $features[] = [
    'type' => 'Feature',
    'geometry' => [
      'type' => 'Point',
      'coordinates' => $coords[0],
    ],
    'properties' => [
      'device' => $DEVICE_ID,
      'from_ts' => $fromTs,
      'to_ts' => $toTs,
      'points' => 1,
      'mode' => $useAbsoluteWindow ? 'absolute' : 'hours',
    ],
  ];
}

if ($includePoints) {
  foreach ($points as $pf) $features[] = $pf;
}

echo json_encode([
  'type' => 'FeatureCollection',
  'features' => $features,
], JSON_UNESCAPED_SLASHES);

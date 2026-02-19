<?php
// gps_batch.php
// Receives JSON array of GPS records, dedupes by timestamp+coords (NOT seq), writes daily CSVs (UTC),
// rotates > RETENTION_DAYS.
// Auth: X-API-Token
// Device ID: ?device=... or header X-Device-Id
// Record fields:
//   seq (optional), ts (required), latE7 (required), lonE7 (required), ch ("wifi"|"lora", optional), net (optional), bat (optional, 0-100), flags (optional, 0-255)

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');

// ================== CONFIG ==================
$BASE_DIR = __DIR__ . '/data';
$RETENTION_DAYS = 7;

// Keep a rolling set of recent keys to dedupe HTTP retries across requests.
// Keys are stored per device in recent_keys.txt (one per line).
$RECENT_KEYS_MAX = 5000;

// Accept timestamps not too far in the future (seconds)
$MAX_FUTURE_SKEW = 86400; // 24h

// Optional: reject extremely old timestamps (comment out if you want to accept deep backfills)
$MIN_TS = 946684800; // 2000-01-01T00:00:00Z

// Set your token here (or load from env)
$EXPECTED_TOKEN = 'CHANGE_ME_LONG_RANDOM_TOKEN';

// ================== AUTH ==================
$token = $_SERVER['HTTP_X_API_TOKEN'] ?? '';
if (!hash_equals($EXPECTED_TOKEN, $token)) {
  http_response_code(401);
  echo json_encode(['ok' => false, 'error' => 'unauthorized']);
  exit;
}

// ================== DEVICE ID ==================
$deviceRaw = $_GET['device'] ?? ($_SERVER['HTTP_X_DEVICE_ID'] ?? 'default');
$DEVICE_ID = preg_replace('/[^a-zA-Z0-9_\-]/', '', (string)$deviceRaw);
if ($DEVICE_ID === '') $DEVICE_ID = 'default';

$devDir = $BASE_DIR . '/' . $DEVICE_ID;
if (!is_dir($devDir) && !mkdir($devDir, 0775, true)) {
  http_response_code(500);
  echo json_encode(['ok' => false, 'error' => 'cannot create device dir']);
  exit;
}

// ================== RETENTION CLEANUP (UTC filenames) ==================
$now = time();
$cutoff = $now - ($RETENTION_DAYS * 86400);

$files = glob($devDir . '/*.csv') ?: [];
foreach ($files as $f) {
  $base = basename($f, '.csv'); // YYYY-MM-DD
  $t = strtotime($base . ' UTC');
  if ($t !== false && $t < $cutoff) {
    @unlink($f);
  }
}

// ================== ACK (timestamp-based) ==================
$ackTsFile = $devDir . '/last_ack_ts.txt';
$lastAckTs = 0;

if (is_file($ackTsFile)) {
  $txt = trim((string)@file_get_contents($ackTsFile));
  if ($txt !== '' && ctype_digit($txt)) $lastAckTs = (int)$txt;
}

// Keep old seq ack for compatibility/debug (not used for dedupe anymore)
$ackSeqFile = $devDir . '/last_ack.txt';
$lastAckSeq = 0;
if (is_file($ackSeqFile)) {
  $txt = trim((string)@file_get_contents($ackSeqFile));
  if ($txt !== '' && ctype_digit($txt)) $lastAckSeq = (int)$txt;
}

// ================== PARSE INPUT ==================
$raw = file_get_contents('php://input');
if ($raw === false || trim($raw) === '') {
  http_response_code(400);
  echo json_encode(['ok' => false, 'error' => 'empty body']);
  exit;
}

$decoded = json_decode($raw, true);
if (!is_array($decoded)) {
  http_response_code(400);
  echo json_encode([
    'ok' => false,
    'error' => 'invalid json',
    'json_error' => function_exists('json_last_error_msg') ? json_last_error_msg() : null,
  ]);
  exit;
}

// ================== AUTO-DETECT FORMAT (ESP32 vs TTN) ==================
$data = [];
$sourceType = 'unknown';

// Check if this is a TTN webhook (has 'uplink_message' key)
if (isset($decoded['uplink_message']) && is_array($decoded['uplink_message'])) {
  $sourceType = 'ttn';
  $payload = $decoded['uplink_message']['decoded_payload'] ?? null;
  
  if (is_array($payload)) {
    // TTN sends a single uplink, wrap in array for consistent processing
    $data = [$payload];
    
    // Try to extract device ID from TTN if not provided in parameters
    if (!$DEVICE_ID || $DEVICE_ID === 'default') {
      $eui = $decoded['end_device_ids']['device_id'] ?? null;
      if ($eui) {
        $testId = preg_replace('/[^a-zA-Z0-9_\-]/', '', (string)$eui);
        if ($testId !== '') {
          $DEVICE_ID = $testId;
          $devDir = $BASE_DIR . '/' . $DEVICE_ID;
          if (!is_dir($devDir) && !mkdir($devDir, 0775, true)) {
            http_response_code(500);
            echo json_encode(['ok' => false, 'error' => 'cannot create device dir']);
            exit;
          }
        }
      }
    }
  } else {
    http_response_code(400);
    echo json_encode(['ok' => false, 'error' => 'TTN format: missing decoded_payload']);
    exit;
  }
} else {
  // Assume direct format from ESP32 (array of records)
  $sourceType = 'esp32';
  // Check if it's a single record object (not array of records)
  if (isset($decoded['ts']) || isset($decoded['latE7']) || isset($decoded['lonE7'])) {
    // Single record, wrap it
    $data = [$decoded];
  } else {
    // Already an array of records
    $data = $decoded;
  }
}

// ================== HELPERS ==================
function isIntLike($v): bool {
  if (is_int($v)) return true;
  if (!is_string($v) && !is_float($v)) return false;
  $s = trim((string)$v);
  if ($s === '') return false;
  if ($s[0] === '-') return ctype_digit(substr($s, 1));
  return ctype_digit($s);
}

function toInt($v): int {
  return (int)$v;
}

function normalizeEpochSeconds(int $ts): int {
  // If milliseconds (or bigger), convert to seconds.
  // 20,000,000,000 seconds ~ year 2600, safely beyond expected device timestamps.
  if ($ts > 20000000000) {
    return (int)round($ts / 1000);
  }
  return $ts;
}

function atomicWrite(string $path, string $content): bool {
  $tmp = $path . '.tmp';
  $ok = @file_put_contents($tmp, $content, LOCK_EX);
  if ($ok === false) return false;
  return @rename($tmp, $path);
}

function loadRecentKeysFromLines(array $lines, int $max): array {
  $set = [];
  $list = [];

  // keep only last $max lines
  $count = count($lines);
  if ($count > $max) $lines = array_slice($lines, $count - $max);

  foreach ($lines as $ln) {
    $ln = trim((string)$ln);
    if ($ln === '') continue;
    if (!isset($set[$ln])) {
      $set[$ln] = true;
      $list[] = $ln;
    }
  }
  return [$set, $list];
}

function appendRecentKey(array &$set, array &$list, string $key, int $max): void {
  if (isset($set[$key])) return;
  $set[$key] = true;
  $list[] = $key;
  while (count($list) > $max) {
    $old = array_shift($list);
    if ($old !== null) unset($set[$old]);
  }
}

// ================== DEDUPE STATE (timestamp-based) with LOCK ==================
$recentKeysFile = $devDir . '/recent_keys.txt';
$rkFp = fopen($recentKeysFile, 'c+');
if ($rkFp === false) {
  http_response_code(500);
  echo json_encode(['ok' => false, 'error' => 'cannot open recent keys']);
  exit;
}
if (!flock($rkFp, LOCK_EX)) {
  fclose($rkFp);
  http_response_code(500);
  echo json_encode(['ok' => false, 'error' => 'cannot lock recent keys']);
  exit;
}
rewind($rkFp);
$rkContent = (string)stream_get_contents($rkFp);
$rkLines = preg_split("/\r\n|\n|\r/", $rkContent) ?: [];
list($recentSet, $recentList) = loadRecentKeysFromLines($rkLines, $RECENT_KEYS_MAX);

// ================== PROCESS RECORDS ==================
$written = 0;
$skippedDup = 0;
$skippedBad = 0;

$maxTsSeen = $lastAckTs;
$maxSeqSeen = $lastAckSeq;

// For dedupe inside this request too
$reqSet = [];

foreach ($data as $rec) {
  if (!is_array($rec)) { $skippedBad++; continue; }

  $seq   = $rec['seq']   ?? 0;
  $ts    = $rec['ts']    ?? null;
  $latE7 = $rec['latE7'] ?? null;
  $lonE7 = $rec['lonE7'] ?? null;
  $net   = $rec['net']   ?? 'unknown';
  $bat   = $rec['bat']   ?? null;

  $ch = $rec['ch'] ?? 'wifi';
  $ch = strtolower((string)$ch);
  if ($ch !== 'wifi' && $ch !== 'lora') $ch = 'wifi';

  if (!isIntLike($ts) || !isIntLike($latE7) || !isIntLike($lonE7)) {
    $skippedBad++; continue;
  }

  $ts    = normalizeEpochSeconds(toInt($ts));
  $latE7 = toInt($latE7);
  $lonE7 = toInt($lonE7);
  $seq   = isIntLike($seq) ? toInt($seq) : 0;
  
  // Validate and clamp battery
  $batVal = '';
  if (isIntLike($bat)) {
    $batVal = toInt($bat);
    if ($batVal < 0) $batVal = 0;
    if ($batVal > 100) $batVal = 100;
  }
  
  // Validate flags
  $flagsVal = '';
  if (isIntLike($flags)) {
    $flagsVal = toInt($flags) & 0xFF;  // keep only 8 bits
  }

  // sanity checks
  if ($ts < 0 || $ts > ($now + $MAX_FUTURE_SKEW)) { $skippedBad++; continue; }
  if ($ts < $MIN_TS) { $skippedBad++; continue; } // comment this line if you want deep backfills
  if ($latE7 < -900000000 || $latE7 >  900000000) { $skippedBad++; continue; }
  if ($lonE7 < -1800000000 || $lonE7 > 1800000000) { $skippedBad++; continue; }

  // Timestamp-based idempotency key (recommendation: exclude net/ch for stable dedupe)
  $key = $ts . ',' . $latE7 . ',' . $lonE7;

  if (isset($reqSet[$key]) || isset($recentSet[$key])) {
    $skippedDup++;
    continue;
  }
  $reqSet[$key] = true;

  $date = gmdate('Y-m-d', $ts);
  $file = $devDir . '/' . $date . '.csv';

  $fp = fopen($file, 'ab');
  if ($fp === false) { $skippedBad++; continue; }

  $okWrite = false;

  if (flock($fp, LOCK_EX)) {
    // header only if file empty (race-safe)
    $stat = fstat($fp);
    $empty = ($stat !== false && (int)($stat['size'] ?? 0) === 0);
    if ($empty) {
      $hdr = "seq,ts_iso,ts_epoch,latE7,lonE7,lat,lon,ch,net,bat,flags\n";
      $r = fwrite($fp, $hdr);
      if ($r === false || $r === 0) {
        flock($fp, LOCK_UN);
        fclose($fp);
        $skippedBad++;
        continue;
      }
    }

    $lat = number_format($latE7 / 1e7, 7, '.', '');
    $lon = number_format($lonE7 / 1e7, 7, '.', '');
    $iso = gmdate('c', $ts);

    $line = $seq . ',' . $iso . ',' . $ts . ',' . $latE7 . ',' . $lonE7 . ',' . $lat . ',' . $lon . ',' . $ch . ',' . $net . ',' . $batVal . ',' . $flagsVal . "\n";
    $r = fwrite($fp, $line);

    if ($r !== false && $r > 0) {
      fflush($fp);
      $okWrite = true;
    }

    flock($fp, LOCK_UN);
  }

  fclose($fp);

  if (!$okWrite) { $skippedBad++; continue; }

  // Mark as written/deduped
  $written++;
  appendRecentKey($recentSet, $recentList, $key, $RECENT_KEYS_MAX);

  if ($ts > $maxTsSeen) $maxTsSeen = $ts;
  if ($seq > $maxSeqSeen) $maxSeqSeen = $seq;
}

// ================== Persist recent keys (under lock) ==================
rewind($rkFp);
ftruncate($rkFp, 0);
fwrite($rkFp, implode("\n", $recentList) . (count($recentList) ? "\n" : ''));
fflush($rkFp);
flock($rkFp, LOCK_UN);
fclose($rkFp);

// ================== Persist acks atomically ==================
if ($maxTsSeen > $lastAckTs) {
  atomicWrite($ackTsFile, (string)$maxTsSeen);
}
if ($maxSeqSeen > $lastAckSeq) {
  atomicWrite($ackSeqFile, (string)$maxSeqSeen);
}

// ================== RESPONSE ==================
echo json_encode([
  'ok' => true,
  'device' => $DEVICE_ID,
  'source' => $sourceType,
  'ackedTs' => $maxTsSeen,
  // compatibility fields
  'ackedSeq' => $maxSeqSeen,
  'written' => $written,
  'skipped_dup' => $skippedDup,
  'skipped_bad' => $skippedBad,
]);

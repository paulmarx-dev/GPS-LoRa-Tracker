/**
 * TTN Payload Formatter for GPS LoRa Tracker
 * 
 * Decodes 13-byte payload format and formats for gps_batch.php compatibility:
 * - Bytes 0-3: Timestamp (uint32_t, seconds since epoch)
 * - Bytes 4-7: Latitude (int32_t, degrees × 1e7)
 * - Bytes 8-11: Longitude (int32_t, degrees × 1e7)
 * - Byte 12: Battery percentage (uint8_t, 0-100%)
 * 
 * Output fields compatible with gps_batch.php:
 * - ts: timestamp (seconds)
 * - latE7: latitude × 1e7 (raw int32_t value)
 * - lonE7: longitude × 1e7 (raw int32_t value)
 * - bat: battery percentage (0-100)
 * - ch: channel ("lora")
 */

function decodeUplink(input) {
  var data = input.bytes;
  var errors = [];
  var warnings = [];

  // Validate payload length
  if (data.length !== 13) {
    errors.push("Expected 13 bytes, got " + data.length);
    return {
      data: {},
      warnings: warnings,
      errors: errors
    };
  }

  try {
    // Decode timestamp (bytes 0-3, big-endian, uint32_t)
    var ts = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    ts = ts >>> 0; // Convert to unsigned

    // Decode latitude (bytes 4-7, big-endian, int32_t)
    // Keep raw value for gps_batch.php (×1e7)
    var latE7 = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    
    // Decode longitude (bytes 8-11, big-endian, int32_t)
    // Keep raw value for gps_batch.php (×1e7)
    var lonE7 = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];

    // Decode battery percentage (byte 12, uint8_t)
    var bat = data[12];

    // Validation checks (using decoded values)
    var latitude = latE7 / 1e7;
    var longitude = lonE7 / 1e7;
    
    if (latitude < -90 || latitude > 90) {
      warnings.push("Latitude out of valid range: " + latitude);
    }
    if (longitude < -180 || longitude > 180) {
      warnings.push("Longitude out of valid range: " + longitude);
    }
    if (bat > 100) {
      warnings.push("Battery percentage exceeds 100%: " + bat);
    }

    return {
      data: {
        ts: ts,
        latE7: latE7,
        lonE7: lonE7,
        bat: bat,
        ch: "lora"
      },
      warnings: warnings,
      errors: errors
    };
  } catch (error) {
    errors.push("Decoding error: " + error.message);
    return {
      data: {},
      warnings: warnings,
      errors: errors
    };
  }
}

/**
 * Optional: Encode function for downlinks (if needed in future)
 * Currently not used but included for completeness
 */
function encodeDownlink(input) {
  return {
    bytes: [],
    fPort: 1,
    warnings: [],
    errors: []
  };
}

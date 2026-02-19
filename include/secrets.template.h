#ifndef SECRETS_H
#define SECRETS_H

// ============= WIFI CREDENTIALS =============
// Don't forget to activate "maximize compatibility" in iPhone hotspot settings

#define SSID_IPHONE   "YOUR_IPHONE_SSID"
#define PASS_IPHONE   "YOUR_IPHONE_PASS"

#define SSID_HOME     "YOUR_HOME_SSID"
#define PASS_HOME     "YOUR_HOME_PASS"

// Access Point (optional)
#define AP_SSID       "YOUR_AP_SSID"
#define AP_PASS       "YOUR_AP_PASS"
#define AP_HOST_NAME  "your-esp32"  // mDNS name: your-esp32.local
                                    // Avoid spaces and special chars

// ============= HTTP UPLOAD CONFIGURATION =============
// Server endpoint for GPS batch uploads

#define UPLOAD_URL "https://your-server.com/path/to/gps_batch.php"
#define HTTP_X_API_TOKEN "CHANGE_ME_LONG_RANDOM_TOKEN"
#define HTTP_X_DEVICE_ID "ESP32-GPS-001"

// ============= LORAWAN CREDENTIALS (OTAA) =============
// Get these from TTN Console: Applications > Your App > End devices
// Format: MSB (big-endian)

// Device EUI (8 bytes) - Unique per device
#define LORAWAN_DEVEUI { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// Join EUI / App EUI (8 bytes) - From TTN join server
#define LORAWAN_JOINEUI { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// Application Key (16 bytes) - Shared secret
#define LORAWAN_APPKEY { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// Network Key (16 bytes) - Link encryption key
#define LORAWAN_NWKKEY { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// Heltec LoRa License (required for Heltec devices)
// Leave as-is for ESP32 devices, optional for other platforms
#define LORAWAN_LICENSE {0x00000000, 0x00000000, 0x00000000, 0x00000000}
// more infos on Heltec licenses
// https://docs.heltec.org/general/how_to_use_license.html
// Licensing website: https://resource.heltec.cn/search/
// Get ChipID Code: https://github.com/HelTecAutomation/Heltec_ESP32/blob/master/examples/ESP32/GetChipID/GetChipID.ino



#endif // SECRETS_H
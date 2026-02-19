# GPS LoRa Tracker 📍

A battery-optimized GPS tracker with **dual-channel uplink**: WiFi (primary, instant) and LoRa (fallback, long-range).

Perfect for tracking vehicles, assets, or adventures with reliable location logging over multiple networks.

## Features

✅ **Dual-Channel Architecture**
- WiFi: Primary uplink when available (instant, unlimited range locally)
- LoRa: Fallback uplink via TTN (long-range, global coverage)
- Intelligent switching: LoRa radio sleeps when WiFi connected (power savings)

✅ **Smart GPS Acquisition**
- Movement detection (speed-based, 2 kmh start threshold)
- Distance-triggered transmission (50m movement threshold)
- Heartbeat transmission every 15 minutes
- Only transmits valid GPS fixes (no empty payloads)

✅ **Power Optimization**
- Dynamic LoRa power management (sleep/resume based on WiFi)
- Efficient ring buffer storage (no memory leaks)
- Selective transmission (events only, not continuous)
- ~<1µA radio sleep current

✅ **Real-Time Monitoring**
- Live transmission stats display (WiFi/LoRa point counts)
- Battery percentage monitoring
- Connection status indicators (WiFi online, LoRa joined)
- Active transmission status

✅ **Cloud Integration**
- Server-side deduplication by (timestamp, lat, lon)
- Daily CSV exports per device
- Configurable retention (default 7 days)
- TTN webhook + local HTTP endpoint support

## Hardware

- **Device**: Heltec WiFi LoRa 32 (V4)
- **Processor**: ESP32-S3 @ 240 MHz
- **Radio**: SX1262 LoRa transceiver
- **Frontend**: GC1109 PA module
- **Display**: 128x64 OLED
- **Power**: USB-C + LiPo battery

## Quick Start

### 1. Clone Repository
```bash
git clone https://github.com/YOUR_USERNAME/GPS-LoRa-Tracker.git
cd GPS-LoRa-Tracker
```

### 2. Configure Credentials
```bash
cp include/secrets.template.h include/secrets.h
```

Edit `include/secrets.h`:
- Add WiFi networks (SSID + password)
- Add LoRaWAN credentials (DEVEUI, JOINEUI, APPKEY, NWKKEY)
- Update server endpoint URLs if using self-hosted backend

### 3. Build & Upload
```bash
# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

### 4. Monitor on OLED
Device displays live stats:
```
DATA & TRANSMISSIONS
1243 | 172 acked | 1071 pending
WiFi: 123 @ 11:02:43 [online]
LoRa: 50 @ 11:45:54 [joined]
[idle]
```

### User Interface Controls

**Display Mode Switching**
- **Short press**: Cycles through display screens (Stats → WLAN → Battery → etc.)
- **Long press (5 seconds)**: Turns off OLED display (power saving)
- **Short press (when off)**: Reactivates display

This allows you to browse different information and extend battery life by disabling the display during long deployments.

## Architecture

### Firmware (ESP32-S3 + SX1262)

**Track Storage** (Ring Buffer)
- Circular buffer: GPS fixes + metadata
- Atomic writes, thread-safe accessors
- No dynamic allocation (memory safe)

**Dual Task Model**
```
loraTask()        ← LoRaWAN TX (movement-based)
uploadTask()      ← WiFi batch upload (30 fixes/batch)
gpsTask()         ← GPS acquisition (TinyGPS++)
uiTask()          ← OLED display refresh
```

**TX Logic**
- LoRa: Selective (movement triggers only)
- WiFi: Batch mode (upload recent unacked fixes)
- Both read from same storage → consistent data

### Backend (PHP + Database)

**gps_batch.php** - Auto-detects format:
- ESP32 direct JSON: `[{ts, latE7, lonE7, bat, ch}]`
- TTN webhook: extracts `uplink_message.decoded_payload`
- Dedupes by `(ts, latE7, lonE7)`
- Writes daily CSVs (UTC): `/data/{device}/{YYYY-MM-DD}.csv`

**Payload Formatter** (TTN)
```javascript
// 13-byte binary → JSON conversion
Bytes 0-3:   timestamp (uint32_t)
Bytes 4-7:   latitude × 1e7 (int32_t)
Bytes 8-11:  longitude × 1e7 (int32_t)
Byte 12:     battery % (uint8_t)
```

## Payload Format

### LoRa (13 bytes)
```
+--------+--------+--------+--------+--------+
| byte 0 | byte 1 | byte 2 | byte 3 | byte 4 |
+--------+--------+--------+--------+--------+
         timestamp (seconds)
                              latitude × 1e7 start →
+--------+--------+--------+--------+--------+
| byte 5 | byte 6 | byte 7 | byte 8 | byte 9 |
+--------+--------+--------+--------+--------+
   ← latitude × 1e7   longitude × 1e7 start →
+--------+--------+--------+--------+--------+
| byte10 | byte11 | byte12 |
+--------+--------+--------+
   ← longitude × 1e7    battery %
```

### WiFi JSON
```json
[{
  "ts": 1771441993,
  "latE7": 523882157,
  "lonE7": 97250058,
  "bat": 73,
  "ch": "wifi",
  "net": "iPhone 15 Paul"
}]
```

## Configuration

### Movement Detection
**File**: `src/lora_manager.cpp`
```cpp
MOVE_START_KMH = 2.0f    // Speed threshold to start transmitting
MOVE_STOP_KMH  = 1.0f    // Speed threshold to pause transmitting
DIST_TRIGGER_M = 50.0f   // Distance in meters to trigger TX
HEARTBEAT_INTERVAL_MS = 15 * 60 * 1000  // 15 minutes
```

### RF Configuration
- Frequency: 868.1 MHz (EU868)
- Spreading Factor: 9
- Bandwidth: 125 kHz
- Coding Rate: 4/7
- TX Power: 22 dBm

### Storage
- Ring buffer capacity: configurable (default 500 fixes)
- ~2.5KB per fix (seq, ts, lat, lon, bat, flags)
- Total: ~1.25 MB for 500 fixes

## Debug Outputs

Serial monitor shows:
```
[LoRaWAN] Initializing...
[SX1262] Initializing radio... SUCCESS
[LoRaWAN] Configuring OTAA...
[JOIN] Retrying activateOTAA...
[JOIN] SUCCESS - Device joined!
[TX] GPS Fix lat=52.381461 lon=9.725351 bat=100%
     Payload: 00 00 00 01 1F ...
[RX] No downlink (normal for unconfirmed)
```

## Troubleshooting

### Device won't join LoRa
- Verify LoRaWAN credentials in `secrets.h`
- Ensure TTN application created with OTAA enabled
- Check antenna connection (external or internal)
- Monitor signal strength near window/outdoors

### WiFi upload slow
- Increase `UPLOAD_INTERVAL_MS` in `upload_manager.h`
- Reduce `MAX_UPLOAD_BATCH_SIZE` if network unreliable
- Check server logs for dropped requests

### GPS won't acquire fix
- Device needs outdoor line-of-sight
- Cold fix takes 30-60 seconds first time
- Warm fix typically 5-15 seconds after
- Check TinyGPS++ serial output

### Battery drains too fast
- Verify LoRa radio sleeps when WiFi connected (check serial output)
- Reduce `HEARTBEAT_INTERVAL_MS` if continuous TX observed
- Check GPS sampling rate (should be 1Hz max)

## Data Export

Daily CSV files in `php/data/{device}/`:
```csv
seq,ts_iso,ts_epoch,latE7,lonE7,lat,lon,ch,net,bat,flags
1,2026-02-18T19:13:13+00:00,1771441993,523882157,97250058,52.3882157,9.7250058,wifi,iPhone 15 Paul,73,
2,2026-02-18T19:14:13+00:00,1771442053,523876842,97249138,52.3876842,9.7249138,wifi,iPhone 15 Paul,72,
```

Import into mapping tools (Google Maps, Leaflet, etc.) for visualization.

## Project Status

✅ Core functionality complete and tested
✅ Dual uplink working (WiFi + LoRa)  
✅ Movement detection + heartbeat implemented
✅ Real-time display stats added
✅ Power optimization (radio sleep) working

⚠️ Known issues
- Device shutdown after 70 points (~2 hours) - investigating memory/watchdog

## License

MIT License - See LICENSE file

## Contributing

Found a bug? Have an improvement idea?

1. Fork the repo
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Commit changes: `git commit -am 'Add feature'`
4. Push: `git push origin feature/your-feature`
5. Open a Pull Request

## Hardware References

### Device Setup & Documentation
- [Heltec WiFi LoRa 32 V4 Docs](https://heltec.org/project/wifi-lora-32-v4/)
- [Heltec ESP32 LoRa V4 on PlatformIO - Community Setup Guide](https://github.com/paulmarx-dev/Heltec-ESP32-LoRa-V4-on-PlatformIO)
- [Heltec LoRa Development Board Guide](https://heltec-guide-adrmjhz9.manus.space/)

### Component Datasheets & Libraries
- [SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1262)
- [RadioLib Documentation](https://jgromes.github.io/RadioLib/)
- [TinyGPS++ Library](https://github.com/mikalhart/TinyGPSPlus)

## Author

Paul Marx  
Built with ❤️ for tracking adventures anywhere, anytime.

---

**Status**: Active Development  
**Last Updated**: February 2026

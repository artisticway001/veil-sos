# VEIL - women's safety SOS

Team Noodle Circuit - Sensors Enclave 2026 (BMSCE IEEE).

```
Keychain --radio--> Bracelet --radio--> Basement ESP32 --wire--> Guard desk (Arduino + LCD)
                       |   ^                                          |
                       |   +---------- HELP COMING (3 buzzes) <-------+
                       +--Bluetooth--> Phone app (index.html, Chrome on Android)
```

No internet needed for the SOS path (ESP-NOW radio, works in basements).

| Folder | Board | What it does |
|---|---|---|
| `veil_bracelet_app/` | ESP32-C3 SuperMini (Partition: Huge APP) | Touch SOS, clasp reed snatch alarm, keychain relay, Bluetooth to phone |
| `veil_keychain/` | ESP32-C3 + latch | One press = SOS, then powers itself off |
| `veil_basement_desk/` | ESP32 WROOM-32 | Catches SOS, tells desk Arduino, sends HELP COMING |
| `veil_desk_arduino/` | Arduino Leonardo + 16x2 LCD | Shows SOS source, buzzer, guard button |
| `index.html` | Phone (Chrome, Android) | Live status, siren, GPS, WhatsApp/SMS/call |

The phone app needs HTTPS for Bluetooth: turn on GitHub Pages (Settings -> Pages -> Deploy from branch `main`, root).

# VEIL - women's safety SOS

Team Noodle Circuit - Sensors Enclave 2026 (BMSCE IEEE).

```
Keychain --radio--> Bracelet --radio--> Basement ESP32 --wire--> Guard desk (Arduino + LCD)
                       |   ^                                          |
                       |   +---------- HELP COMING (3 buzzes) <-------+
                       +--Bluetooth--> Phone app (index.html, Chrome on Android)
```

No internet needed for the SOS path (ESP-NOW radio, works in basements).

## FINAL set - upload these 4 (+ phone app)

| Board | File | Arduino board setting |
|---|---|---|
| Bracelet (ESP32-C3) | `veil_bracelet_app/veil_bracelet_app.ino` | ESP32C3 Dev Module, USB CDC On Boot = Enabled, Partition = Huge APP |
| Keychain (ESP32-C3) | `veil_keychain/veil_keychain.ino` | ESP32C3 Dev Module, USB CDC On Boot = Enabled |
| Basement receiver (ESP32 WROOM-32) | `veil_basement_desk/veil_basement_desk.ino` | ESP32 Dev Module |
| Guard desk (Leonardo, LED + button, no buzzer) | `veil_desk_led/veil_desk_led.ino` | Arduino Leonardo |
| Phone | `index.html` (GitHub Pages link, Chrome on Android) | - |

Other desk folders (`veil_desk_simple`, `veil_desk_i2c`, `veil_desk_arduino`) are older variants.

| Folder | Board | What it does |
|---|---|---|
| `veil_bracelet_app/` | ESP32-C3 SuperMini (Partition: Huge APP) | Touch SOS, clasp reed snatch alarm, keychain relay, Bluetooth to phone |
| `veil_keychain/` | ESP32-C3 + latch | One press = SOS, then powers itself off |
| `veil_basement_desk/` | ESP32 WROOM-32 | Catches SOS, tells desk Arduino, sends HELP COMING |
| `veil_desk_arduino/` | Arduino Leonardo + 16x2 LCD | Shows SOS source, buzzer, guard button |
| `veil_desk_simple/` | Arduino Leonardo (no LCD) | Buzzer + LED + guard button only |
| `veil_desk_i2c/` | Arduino Leonardo + I2C LCD | LCD (4 wires) + buzzer + LED + guard button |
| `index.html` | Phone (Chrome, Android) | Live status, siren, GPS, WhatsApp/SMS/call |

The phone app needs HTTPS for Bluetooth: turn on GitHub Pages (Settings -> Pages -> Deploy from branch `main`, root).

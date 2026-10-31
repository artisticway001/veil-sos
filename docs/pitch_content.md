# VEIL - Round II slide content (Sensors Enclave 2026)

## 1. TITLE
- **VEIL** - Silent SOS that works where phones don't
- Wearable + keychain safety system for basements, parking lots and no-signal zones
- Team Noodle Circuit - Bharat & Amulya
- Sensors Enclave 2026 - BMSCE IEEE

## 2. PROBLEM UNDERSTANDING
**What problem?**
- Most safety apps and SOS buttons need a phone, mobile data or GPS.
- Underground parking, basements, lifts and stairwells often have **no signal** - exactly where women feel most unsafe.
- In a real threat, unlocking a phone and opening an app is slow and visible to the attacker.

**Who faces it?**
- Women working late in offices, malls, hospitals, apartments, college campuses.
- Anyone using basement parking or isolated service areas.

**Why it matters**
- The help is usually nearby (a security guard on the same premises) but has no way to know.
- Seconds matter: a silent, one-touch alert that reaches the guard directly can stop an incident.

## 3. PROPOSED SOLUTION
**Approach:** a direct radio link (ESP-NOW) from the user to the security guard - no internet, no SIM, no phone needed.

**Key features (built)**
- **Bracelet:** hold the touch pad 1 s = silent SOS; vibration confirms it was sent.
- **Snatch alarm:** magnetic clasp with reed switch - if the bracelet is pulled off, SOS is sent automatically (with a 5 s cancel window).
- **Keychain:** one press = SOS; it powers itself off afterwards (latch circuit, near-zero standby battery use).
- **Relay:** the bracelet passes on the keychain's SOS, extending range.
- **Guard desk:** LCD shows "SOS ALERT - FROM: BRACELET / KEYCHAIN", buzzer + LED alarm.
- **Two-way:** guard presses one button -> bracelet vibrates 3 times = "help is coming".
- **Phone app (bonus):** Bluetooth web app shows live status, plays a siren, gets GPS and shares location on WhatsApp/SMS when signal is available.

**What makes it different**
- Works with **zero network** - designed for basements.
- **Silent and discreet** - vibration feedback, no loud phone interaction.
- **Closed loop** - she knows help is coming (most SOS devices are one-way).
- Every SOS sent 8 times + resent every 10 s until answered = reliable.
- Low cost (~Rs 2,500 for the full prototype set).

## 4. SYSTEM ARCHITECTURE
**Block diagram**
```
Keychain --ESP-NOW--> Bracelet --ESP-NOW--> Basement ESP32 --wire--> Guard desk (Leonardo + LCD)
                         |  ^                                              |
                         |  +-------- HELP COMING (3 vibrations) <---------+
                         +--Bluetooth--> Phone app (status, siren, GPS share)
```
**Working principle**
1. User holds touch pad / presses keychain / clasp is pulled open.
2. Device broadcasts a 4-byte SOS packet over ESP-NOW (2.4 GHz, channel 1), 8 times.
3. Basement receiver catches it and sends "SOS 1/2" over a serial wire to the guard desk every second (also a heartbeat - desk shows "NO BASEMENT LINK" if a wire is loose).
4. Guard desk: LCD + buzzer + LED alarm, shows which device sent it.
5. Guard presses button -> receiver broadcasts HELP COMING -> bracelet vibrates 3x.

**Sensor integration**
- TTP223 capacitive touch sensor - SOS trigger (hold 1 s to avoid false alarms).
- Reed switch + magnet in clasp - snatch / removal detection.
- Coin vibration motor - silent haptic feedback (different patterns per event).
- Push button with self-power latch - keychain trigger.

## 5. HARDWARE AND TECHNOLOGY STACK
| Part | Component | Approx cost (Rs) |
|---|---|---|
| Bracelet | ESP32-C3 SuperMini, TTP223 touch, coin vibration motor + BC547 + 1N4007, reed switch + magnet, TP4056 charger, 3.7 V LiPo, slide switch | ~700 |
| Keychain | ESP32-C3 SuperMini, push button, MOSFET + BC547 power latch, LiPo | ~500 |
| Basement receiver | ESP32 WROOM-32 DevKit | ~450 |
| Guard desk | Arduino Leonardo, 16x2 I2C LCD, buzzer, LED, push button, power bank | ~1,000 |
| **Total prototype** | | **~2,600** |

- **Microcontrollers:** ESP32-C3 (bracelet, keychain), ESP32 WROOM-32 (receiver), Arduino Leonardo (desk).
- **Communication:** ESP-NOW (device-to-device, no router), UART serial (receiver -> desk), BLE (bracelet -> phone).
- **Software:** Arduino IDE (C++), ESP-NOW + BLE libraries, LiquidCrystal I2C; phone app = HTML/JavaScript Web Bluetooth (Chrome, Android), hosted on GitHub Pages.
- Code: github.com/artisticway001/veil-sos

## 6. IMPLEMENTATION AND DEMONSTRATION
**Prototype photographs:** bracelet board, keychain board, basement ESP32, guard desk with LCD. (add photos)

**Workflow (live demo)**
1. Hold bracelet touch pad -> 1 vibration -> desk LCD: "SOS ALERT FROM: BRACELET", buzzer double-beeps.
2. Press keychain -> bracelet vibrates 2x (relay) -> desk: "FROM: KEYCHAIN".
3. Guard presses button -> LCD "HELP IS COMING" -> bracelet vibrates 3x.
4. Pull the clasp open -> warning pulses -> automatic SOS.
5. Phone app turns red with siren and location share.

**Testing done**
- Bracelet -> basement receiver: working.
- Keychain -> bracelet -> receiver relay: working.
- Receiver -> desk serial link: working ("SOS 1" received and displayed).
- Motor, touch sensor, LCD, buzzer each verified with Serial Monitor test commands.
- Debugged: motor stuck ON (pin mismatch), false touch triggers (wiring), loose connections.
- (fill in: range test result in basement - metres, packets received out of 8)

## 7. RESULTS, IMPACT AND FUTURE SCOPE
**Performance**
- Alert reaches guard desk in under 1 second (fill in your measured value).
- 8x repeated packets + 10 s resend until acknowledged.
- Keychain uses power only while sending (self power-off latch).

**Limitations**
- Range limited by concrete walls - large basements need relay nodes.
- Prototype on perfboard - not yet waterproof or miniaturised.
- Phone app needs Android Chrome; location sharing needs mobile signal.
- Guard must be at the desk to respond.

**Practical applications**
- Office and mall basement parking, apartments, hospitals, hostels and college campuses, factories and night-shift workplaces.

**Improvements (future)**
- Custom PCB + 3D-printed enclosure (CAD already designed).
- Mesh of relay nodes for full-building coverage + location of which zone the SOS came from.
- Tap patterns: 2 taps = check-in, 3 taps = fake call.
- Guard mobile alerts + auto-escalation if not answered in 60 s.
- Heart-rate / fall detection for automatic SOS.

## 8. THANK YOU
- VEIL - because help should reach you, even with no signal.
- Team Noodle Circuit - Bharat & Amulya
- Code + docs: github.com/artisticway001/veil-sos

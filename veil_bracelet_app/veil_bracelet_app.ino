// VEIL BRACELET v5 = v4 + desk-link fixes for Bluetooth (radio channel re-set, Bluetooth pauses while sending,
//                    faster repeats).  USE_APP 0 = no Bluetooth (desk link exactly like the version that worked)
// Everything from v2 works exactly the same. Bluetooth only ADDS the phone app.
//
// Arduino Tools menu:
//   Board            = ESP32C3 Dev Module
//   USB CDC On Boot  = Enabled
//   Partition Scheme = Huge APP (3MB No OTA/1MB SPIFFS)   <-- NEEDED (Wi-Fi + Bluetooth is big)
//
// Hold touch pad 1 s (or BOOT button) -> SOS to basement -> 1 long buzz
// Keychain SOS heard -> 2 short buzzes + passes it on
// Guard presses button -> HELP COMING -> 3 long buzzes
// Clasp pulled open (worn for 5 s first) -> 5 s warning buzz -> SOS.  Touch pad or re-close clasp = cancel
// Phone app: shows everything live, can test-buzz, send SOS, cancel
// Serial 115200: t = test SOS, m = motor test, c = clasp status, r = reed alarm on/off

#define USE_APP 1     // 1 = phone app ON.  If the desk still gets nothing, set 0 and upload.

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#if USE_APP
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#endif

// ---- pins: keep the numbers that WORK on your board ----
#define MOTOR_PIN 5   // -> 1k -> BC547 base
#define TOUCH_PIN 3   // TTP223 SIG (HIGH when touched)
#define BOOT_BTN  9   // backup button
#define REED_PIN  6   // reed switch: GPIO6 -> reed -> GND (magnet near = clasp closed)
#define CHANNEL   1
#define MY_ID     2

// ---- Bluetooth IDs (the app uses the same ones) ----
#define SVC_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"   // phone -> bracelet
#define TX_UUID  "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   // bracelet -> phone

typedef struct { uint8_t magic, type, id, count; } Msg;
uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

volatile bool gotHelp = false, gotKey = false;
bool sosActive = false;
uint8_t sosId = MY_ID;
unsigned long sosStart = 0, lastSend = 0, holdStart = 0;
int sendCount = 0;

// clasp / reed
bool reedOn = true, claspClosed = false, armed = false, warning = false, blockHold = false;
unsigned long closedSince = 0, warnStart = 0;

// ---------- Bluetooth (phone app) ----------
volatile bool bleConnected = false;
bool wasConnected = false;
volatile char appCmd = 0;
unsigned long lastBeat = 0;

#if USE_APP
BLECharacteristic *txChar = nullptr;
class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) { bleConnected = true; }
  void onDisconnect(BLEServer *s) { bleConnected = false; }
};
class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    String v = String(c->getValue().c_str()); v.trim();
    if (v == "BUZZ") appCmd = 'b';
    else if (v == "SOS") appCmd = 's';
    else if (v == "STOP") appCmd = 'x';
    else if (v == "PING") appCmd = 'p';
  }
};
#endif

void phone(const char *s, bool quiet = false) {
  if (!quiet) Serial.printf("[app] %s\n", s);
#if USE_APP
  if (bleConnected && txChar) { txChar->setValue((uint8_t *)s, strlen(s)); txChar->notify(); }
#endif
}
const char *stateText() { return !sosActive ? "READY" : (sosId == 1 ? "SOS:1" : "SOS:2"); }

#if USE_APP
void setupBLE() {
  BLEDevice::init("VEIL-Bracelet");
  BLEServer *srv = BLEDevice::createServer();
  srv->setCallbacks(new ServerCB());
  BLEService *svc = srv->createService(SVC_UUID);
  txChar = svc->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());
  BLECharacteristic *rx = svc->createCharacteristic(RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new RxCB());
  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->setScanResponse(true);
  adv->setMinInterval(800);      // slower advertising = more radio time for the desk
  adv->setMaxInterval(1200);
  BLEDevice::startAdvertising();
  Serial.println("Bluetooth ON: VEIL-Bracelet");
}
#endif

// ---------- radio (basement + keychain) - same as v2 ----------
void onPacket(const uint8_t *d, int len) {
  if (len != sizeof(Msg) || d[0] != 'V') return;
  if (d[1] == 2 && (d[2] == MY_ID || d[2] == 1)) gotHelp = true;   // HELP COMING
  else if (d[1] == 1 && d[2] == 1) gotKey = true;                  // keychain SOS
}
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t *i, const uint8_t *d, int len) { onPacket(d, len); }
#else
void onRecv(const uint8_t *mac, const uint8_t *d, int len) { onPacket(d, len); }
#endif

void buzz(int ms) { digitalWrite(MOTOR_PIN, HIGH); delay(ms); digitalWrite(MOTOR_PIN, LOW); }

void sendSOS() {
#if USE_APP
  if (!bleConnected) BLEDevice::getAdvertising()->stop();   // radio fully free for the desk
#endif
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);      // make sure we are on the desk's channel
  int ok = 0;
  for (uint8_t i = 0; i < 8; i++) {
    Msg m = {'V', 1, sosId, i};
    if (esp_now_send(BCAST, (uint8_t *)&m, sizeof(m)) == ESP_OK) ok++;
    delay(40);
  }
#if USE_APP
  if (!bleConnected) BLEDevice::startAdvertising();
#endif
  lastSend = millis(); sendCount++;
  uint8_t ch; wifi_second_chan_t sc; esp_wifi_get_channel(&ch, &sc);
  Serial.printf("SOS sent to desk (id=%d, %d/8 ok, channel %d, try %d)\n", sosId, ok, ch, sendCount);
}
void startSOS(uint8_t id) {
  sosId = id; sosActive = true; sosStart = millis(); sendCount = 0;
  sendSOS();
  phone(id == 1 ? "SOS:1" : "SOS:2");
}

void setup() {
  pinMode(MOTOR_PIN, OUTPUT); digitalWrite(MOTOR_PIN, LOW);   // motor OFF first
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  pinMode(REED_PIN, INPUT_PULLUP);
  Serial.begin(115200); delay(500);

  WiFi.mode(WIFI_STA); WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) Serial.println("ESP-NOW FAILED");
  esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = CHANNEL; p.encrypt = false;
  esp_now_add_peer(&p); esp_now_register_recv_cb(onRecv);

#if USE_APP
  setupBLE();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);   // re-set channel after Bluetooth starts
#endif
  uint8_t ch; wifi_second_chan_t sc; esp_wifi_get_channel(&ch, &sc);
  Serial.printf("Radio channel: %d (desk uses %d)\n", ch, CHANNEL);

  Serial.println("VEIL bracelet ready. Hold touch 1 s = SOS. t = test SOS, m = motor test");
  buzz(200);
}

void loop() {
  // touch debug (matches the red light on the module)
  static int lastT = -1; int t = digitalRead(TOUCH_PIN);
  if (t != lastT) { lastT = t; Serial.println(t ? "TOUCH ON" : "touch off"); }

  // hold 1 s -> SOS
  bool touched = t == HIGH || digitalRead(BOOT_BTN) == LOW;
  if (!touched) { holdStart = 0; blockHold = false; }
  else if (holdStart == 0) holdStart = millis();
  else if (!sosActive && !warning && !blockHold && millis() - holdStart > 1000) { Serial.println("Touch held -> SOS"); startSOS(MY_ID); buzz(800); }

  // ---- clasp reed switch ----
  static int lastR = -1; static unsigned long rChange = 0;
  int r = digitalRead(REED_PIN);                       // LOW = magnet near = clasp closed
  if (r != lastR && millis() - rChange > 50) {
    rChange = millis(); lastR = r;
    claspClosed = (r == LOW);
    Serial.println(claspClosed ? "CLASP CLOSED" : "CLASP OPEN");
    phone(claspClosed ? "CLASP:CLOSED" : "CLASP:OPEN");
    if (claspClosed) {
      closedSince = millis();
      if (warning) { warning = false; digitalWrite(MOTOR_PIN, LOW); Serial.println("Clasp closed again -> cancelled"); buzz(100); }
    } else if (armed && reedOn && !sosActive) {
      warning = true; warnStart = millis();
      Serial.println("CLASP PULLED! SOS in 5 s - touch pad to cancel");
    }
    armed = false;
  }
  if (claspClosed && !armed && millis() - closedSince > 5000) { armed = true; Serial.println("Clasp ARMED (snatch alarm on)"); buzz(80); }

  if (warning) {
    digitalWrite(MOTOR_PIN, ((millis() - warnStart) / 250) % 4 == 0);   // warning pulses
    if (touched && millis() - warnStart > 300) {
      warning = false; blockHold = true; digitalWrite(MOTOR_PIN, LOW);
      Serial.println("Cancelled by touch (taking it off)"); buzz(100);
    } else if (millis() - warnStart > 5000) {
      warning = false; digitalWrite(MOTOR_PIN, LOW);
      Serial.println("SNATCH -> SOS"); phone("SNATCH"); startSOS(MY_ID); buzz(800);
    }
  }

  // serial test commands
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' && !sosActive) { startSOS(MY_ID); buzz(800); }
    if (c == 'm') { Serial.println("motor 1 s"); buzz(1000); }
    if (c == 'c') Serial.printf("Clasp: %s | armed: %s | reed alarm: %s\n", claspClosed ? "CLOSED" : "OPEN", armed ? "yes" : "no", reedOn ? "ON" : "OFF");
    if (c == 'r') { reedOn = !reedOn; warning = false; digitalWrite(MOTOR_PIN, LOW); Serial.printf("Reed alarm %s\n", reedOn ? "ON" : "OFF"); }
  }

  // phone app commands
  if (appCmd) {
    char c = appCmd; appCmd = 0;
    if (c == 'b') { buzz(300); delay(200); buzz(300); phone("BUZZED"); }
    else if (c == 's' && !sosActive) { Serial.println("SOS from phone app"); startSOS(MY_ID); buzz(800); }
    else if (c == 'x' && sosActive) { sosActive = false; Serial.println("SOS cancelled from app"); buzz(100); phone("READY"); }
    else if (c == 'p') phone(stateText());
  }

  // keychain heard -> pass it on
  if (gotKey) { gotKey = false;
    if (!sosActive) { Serial.println("Keychain SOS heard -> passing on"); buzz(150); delay(150); buzz(150); startSOS(1); } }

  // guard answered
  if (gotHelp) { gotHelp = false;
    if (sosActive) {
      sosActive = false; Serial.println("*** HELP IS COMING ***"); phone("HELP");
      for (int i = 0; i < 3; i++) { buzz(600); delay(300); }
    } }

  if (sosActive && millis() - lastSend > (sendCount < 10 ? 3000UL : 10000UL)) sendSOS();          // every 3 s, later 10 s
  if (sosActive && millis() - sosStart > 120000) { sosActive = false; Serial.println("No answer - stopped"); phone("NOANSWER"); }

  // Bluetooth housekeeping
#if USE_APP
  if (bleConnected && !wasConnected) { wasConnected = true; Serial.println("Phone connected"); delay(500); phone(stateText()); }
  if (!bleConnected && wasConnected) { wasConnected = false; Serial.println("Phone disconnected"); delay(300); BLEDevice::startAdvertising(); }
  if (bleConnected && millis() - lastBeat > 3000) { lastBeat = millis(); if (!sosActive) phone("READY", true); }
#endif

  delay(10);
}

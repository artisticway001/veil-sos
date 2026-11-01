// VEIL BRACELET - BASIC (no Bluetooth / no app)  - the version that worked
// Board: ESP32C3 Dev Module, USB CDC On Boot = Enabled
// Hold touch pad 1 s (or BOOT) -> SOS to basement -> 1 long buzz
// Keychain SOS heard -> 2 short buzzes + passes it on
// Guard presses button -> HELP COMING -> 3 long buzzes
// Serial 115200: t = test SOS, m = motor test
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define MOTOR_PIN 5   // GPIO5 -> 1k -> transistor base
#define TOUCH_PIN 3   // TTP223 SIG (HIGH when touched)
#define BOOT_BTN  9   // backup button
#define CHANNEL   1
#define MY_ID     2

typedef struct { uint8_t magic, type, id, count; } Msg;
uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

volatile bool gotHelp = false, gotKey = false;
bool sosActive = false;
uint8_t sosId = MY_ID;
unsigned long sosStart = 0, lastSend = 0, holdStart = 0;

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
  for (uint8_t i = 0; i < 8; i++) { Msg m = {'V', 1, sosId, i}; esp_now_send(BCAST, (uint8_t*)&m, sizeof(m)); delay(25); }
  lastSend = millis();
  Serial.printf("SOS sent (id=%d)\n", sosId);
}
void startSOS(uint8_t id) { sosId = id; sosActive = true; sosStart = millis(); sendSOS(); }

void setup() {
  pinMode(MOTOR_PIN, OUTPUT); digitalWrite(MOTOR_PIN, LOW);   // motor OFF first
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  Serial.begin(115200); delay(500);
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) Serial.println("ESP-NOW FAILED");
  esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = CHANNEL; p.encrypt = false;
  esp_now_add_peer(&p); esp_now_register_recv_cb(onRecv);
  Serial.println("VEIL bracelet (basic) ready. Hold touch 1 s = SOS. t = test SOS, m = motor test");
  buzz(200);
}

void loop() {
  static int lastT = -1; int t = digitalRead(TOUCH_PIN);
  if (t != lastT) { lastT = t; Serial.println(t ? "TOUCH ON" : "touch off"); }

  bool touched = t == HIGH || digitalRead(BOOT_BTN) == LOW;
  if (!touched) holdStart = 0;
  else if (holdStart == 0) holdStart = millis();
  else if (!sosActive && millis() - holdStart > 1000) { Serial.println("Touch held -> SOS"); startSOS(MY_ID); buzz(800); }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' && !sosActive) { startSOS(MY_ID); buzz(800); }
    if (c == 'm') { Serial.println("motor 1 s"); buzz(1000); }
  }

  if (gotKey) { gotKey = false;
    if (!sosActive) { Serial.println("Keychain SOS heard -> passing on"); buzz(150); delay(150); buzz(150); startSOS(1); } }

  if (gotHelp) { gotHelp = false;
    if (sosActive) { sosActive = false; Serial.println("*** HELP IS COMING ***"); for (int i = 0; i < 3; i++) { buzz(600); delay(300); } } }

  if (sosActive && millis() - lastSend > 10000) sendSOS();                                       // resend every 10 s
  if (sosActive && millis() - sosStart > 120000) { sosActive = false; Serial.println("No answer - stopped"); }
  delay(10);
}

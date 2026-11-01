// VEIL BASEMENT -> DESK  (ESP32 WROOM-32, board "ESP32 Dev Module")
// Powered from the Arduino (Arduino 5V -> ESP VIN, GND -> GND)
// Radio SOS heard -> tells the Arduino over ONE wire (GPIO17 -> Arduino A1, 9600 baud)
//   sends every 1 s:  "IDLE"  or  "SOS 1" (keychain) / "SOS 2" (bracelet)   (= heartbeat too)
// Guard presses Arduino button -> Arduino pulls GPIO27 LOW -> HELP COMING sent to bracelet, "ACK" to Arduino
// ESP BOOT button also works as guard button. Serial 115200: t = fake SOS, a = answer
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define CHANNEL 1
const int DESK_TX = 17, DESK_RX = 16, ACK_IN = 27, LED = 2, BOOT_BTN = 0;

typedef struct { uint8_t magic, type, id, count; } Msg;
uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

volatile bool gotSOS = false; volatile uint8_t sosId = 0;
bool alarmOn = false; uint8_t alarmId = 0;
unsigned long ackTime = 0, lastTell = 0;

void onPacket(const uint8_t *d, int len) {
  if (len == sizeof(Msg) && d[0] == 'V' && d[1] == 1) { sosId = d[2]; gotSOS = true; }
}
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t *i, const uint8_t *d, int len) { onPacket(d, len); }
#else
void onRecv(const uint8_t *mac, const uint8_t *d, int len) { onPacket(d, len); }
#endif

void tellDesk() {
  lastTell = millis();
  if (alarmOn) { Serial2.print("SOS "); Serial2.println(alarmId); }
  else Serial2.println("IDLE");
}
void startAlarm(uint8_t id) {
  if (alarmOn || (ackTime && millis() - ackTime < 3000)) return;   // ignore leftover repeats
  alarmOn = true; alarmId = id;
  Serial.printf("*** SOS from %s -> told desk ***\n", id == 1 ? "KEYCHAIN" : id == 2 ? "BRACELET" : "TEST");
  tellDesk();
}
void ackAlarm() {
  alarmOn = false; ackTime = millis(); digitalWrite(LED, LOW);
  for (uint8_t i = 0; i < 5; i++) { Msg m = {'V', 2, alarmId, i}; esp_now_send(BCAST, (uint8_t *)&m, sizeof(m)); delay(20); }
  Serial2.println("ACK");
  Serial.println("Guard answered -> HELP COMING sent to bracelet");
  delay(200); tellDesk();
}

void setup() {
  pinMode(LED, OUTPUT); pinMode(ACK_IN, INPUT_PULLUP); pinMode(BOOT_BTN, INPUT_PULLUP);
  Serial.begin(115200); delay(300);
  Serial2.begin(9600, SERIAL_8N1, DESK_RX, DESK_TX);
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW FAILED"); return; }
  esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = CHANNEL; p.encrypt = false;
  esp_now_add_peer(&p); esp_now_register_recv_cb(onRecv);
  Serial.println("VEIL basement (desk version) ready - waiting for SOS");
}

void loop() {
  if (gotSOS) { gotSOS = false; startAlarm(sosId); }
  if (Serial.available()) { char c = Serial.read(); if (c == 't') startAlarm(9); if (c == 'a' && alarmOn) ackAlarm(); }

  if (alarmOn) {
    digitalWrite(LED, (millis() / 250) % 2);
    if (digitalRead(ACK_IN) == LOW || digitalRead(BOOT_BTN) == LOW) {
      delay(50);
      if (digitalRead(ACK_IN) == LOW || digitalRead(BOOT_BTN) == LOW) ackAlarm();
    }
  } else digitalWrite(LED, (millis() % 3000) < 60);

  if (millis() - lastTell > 1000) tellDesk();   // heartbeat: Arduino knows the wire is OK
  delay(5);
}

// =====================================================================
// VEIL BASEMENT RECEIVER - FINAL  (ESP32 WROOM-32, board "ESP32 Dev Module")
// =====================================================================
// Powered from the Leonardo:  Leonardo 5V -> ESP VIN,  GND -> GND
// GPIO17 (TX2) -> Leonardo D0 (RX)     sends every 1 s: "IDLE" or "SOS 1" / "SOS 2", and "ACK" once
// GPIO27 <- Leonardo D10 (via 1k)      Leonardo pulls it LOW when the guard presses the button
//
// SOS heard  -> tells the desk (LED blinks there)
// Guard button -> HELP COMING sent to bracelet (10 packets)
// If the bracelet missed HELP COMING and keeps calling, it is re-sent automatically (no new alarm).
// ESP BOOT button = guard button too.   Serial 115200:  t = fake SOS   a = answer
// =====================================================================
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define CHANNEL 1
const int DESK_TX = 17, DESK_RX = 16, ACK_IN = 27, LED = 2, BOOT_BTN = 0;
const unsigned long REACK_WINDOW = 20000;     // 20 s after an answer: same caller = re-send HELP

typedef struct { uint8_t magic, type, id, count; } Msg;
uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

volatile bool gotSOS = false;
volatile uint8_t sosId = 0;
volatile int pkts = 0;

bool alarmOn = false;
uint8_t alarmId = 0, lastAckId = 0;
unsigned long ackTime = 0, lastReack = 0, lastTell = 0;

void onPacket(const uint8_t *d, int len) {
  if (len == sizeof(Msg) && d[0] == 'V' && d[1] == 1) { sosId = d[2]; gotSOS = true; pkts++; }
}
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t *info, const uint8_t *d, int len) { onPacket(d, len); }
#else
void onRecv(const uint8_t *mac, const uint8_t *d, int len) { onPacket(d, len); }
#endif

String who(int id) { return id == 1 ? "KEYCHAIN" : id == 2 ? "BRACELET" : "TEST"; }

void tellDesk() {
  lastTell = millis();
  if (alarmOn) { Serial2.print("SOS "); Serial2.println(alarmId); }
  else Serial2.println("IDLE");
}

void sendHelp(uint8_t id) {
  for (uint8_t i = 0; i < 10; i++) {
    Msg m = {'V', 2, id, i};
    esp_now_send(BCAST, (uint8_t *)&m, sizeof(m));
    delay(50);
  }
}

void startAlarm(uint8_t id) {
  if (alarmOn) return;
  // bracelet / keychain still calling right after an answer = it missed HELP COMING
  if (ackTime && millis() - ackTime < REACK_WINDOW && (id == lastAckId || millis() - ackTime < 3000)) {
    if (millis() - lastReack > 2000) {
      lastReack = millis();
      Serial.printf("%s still calling -> re-sending HELP COMING\n", who(id).c_str());
      sendHelp(id);
    }
    return;
  }
  alarmOn = true; alarmId = id;
  Serial.printf("*** SOS from %s -> desk alerted ***\n", who(id).c_str());
  tellDesk();
}

void answer() {
  alarmOn = false; ackTime = millis(); lastAckId = alarmId; digitalWrite(LED, LOW);
  sendHelp(alarmId);
  Serial2.println("ACK");
  Serial.printf("Guard answered -> HELP COMING sent to %s\n", who(alarmId).c_str());
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
  esp_now_add_peer(&p);
  esp_now_register_recv_cb(onRecv);
  Serial.println("\nVEIL basement receiver ready - waiting for SOS   (t = fake SOS, a = answer)");
}

void loop() {
  if (gotSOS) {
    gotSOS = false;
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) { lastPrint = millis(); Serial.printf("radio: SOS packets from %s (total %d)\n", who(sosId).c_str(), pkts); }
    startAlarm(sosId);
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') { ackTime = 0; startAlarm(9); }
    if (c == 'a' && alarmOn) answer();
  }

  if (alarmOn) {
    digitalWrite(LED, (millis() / 250) % 2);
    if (digitalRead(ACK_IN) == LOW || digitalRead(BOOT_BTN) == LOW) {
      delay(50);
      if (digitalRead(ACK_IN) == LOW || digitalRead(BOOT_BTN) == LOW) answer();
    }
  } else digitalWrite(LED, (millis() % 3000) < 60);

  if (millis() - lastTell > 1000) tellDesk();     // heartbeat
  delay(5);
}

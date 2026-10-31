// VEIL KEYCHAIN - clean v2 (ESP32-C3, latch on GPIO4)
// Button powers it -> holds power -> sends SOS 8x -> lets go -> power OFF
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define LATCH_PIN 4
#define CHANNEL   1
#define MY_ID     1

typedef struct { uint8_t magic, type, id, count; } Msg;
uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void setup() {
  pinMode(LATCH_PIN, OUTPUT); digitalWrite(LATCH_PIN, HIGH);   // hold power FIRST
  Serial.begin(115200); delay(200);
  Serial.println("VEIL keychain ON");
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = CHANNEL; p.encrypt = false;
    esp_now_add_peer(&p);
    for (uint8_t i = 0; i < 8; i++) { Msg m = {'V', 1, MY_ID, i}; esp_now_send(BCAST, (uint8_t*)&m, sizeof(m)); delay(25); }
    Serial.println("SOS sent");
  } else Serial.println("ESP-NOW FAILED");
  delay(300);
  Serial.println("Power OFF (stays on if USB plugged in - press RST to send again)");
  Serial.flush();
  digitalWrite(LATCH_PIN, LOW);
  while (true) delay(1000);
}
void loop() {}

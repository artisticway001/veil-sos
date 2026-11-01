// VEIL KEYCHAIN - DIRECT BATTERY (no latch)   Board: ESP32C3 Dev Module, USB CDC On Boot = Enabled
// Wiring:  LiPo +  -> (slide switch) -> C3 "5V" pin      LiPo -  -> C3 GND
//          Button: GPIO3 -> button -> GND                (BOOT button also works for testing)
// Press button -> SOS sent 8x -> blue LED blinks.  Board stays on (switch it off with the slide switch).
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define BTN_PIN   3      // GPIO3 -> button -> GND
#define BOOT_BTN  9      // on-board BOOT button (test)
#define LED_PIN   8      // on-board blue LED (LOW = on)
#define CHANNEL   1
#define MY_ID     1      // 1 = keychain

typedef struct { uint8_t magic, type, id, count; } Msg;
uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
unsigned long lastSOS = 0;

void sendSOS() {
  digitalWrite(LED_PIN, LOW);
  for (uint8_t i = 0; i < 8; i++) { Msg m = {'V', 1, MY_ID, i}; esp_now_send(BCAST, (uint8_t*)&m, sizeof(m)); delay(25); }
  digitalWrite(LED_PIN, HIGH);
  Serial.println("SOS sent");
}

void setup() {
  setCpuFrequencyMhz(80);                 // saves battery
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200); delay(300);
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) Serial.println("ESP-NOW FAILED");
  esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = CHANNEL; p.encrypt = false;
  esp_now_add_peer(&p);
  Serial.println("VEIL keychain (direct battery) ready - press button = SOS");
  for (int i = 0; i < 2; i++) { digitalWrite(LED_PIN, LOW); delay(100); digitalWrite(LED_PIN, HIGH); delay(100); }
}

void loop() {
  bool pressed = digitalRead(BTN_PIN) == LOW || digitalRead(BOOT_BTN) == LOW;
  if (pressed && millis() - lastSOS > 3000) {      // max one SOS every 3 s
    delay(30);
    if (digitalRead(BTN_PIN) == LOW || digitalRead(BOOT_BTN) == LOW) { lastSOS = millis(); sendSOS(); }
  }
  delay(20);
}

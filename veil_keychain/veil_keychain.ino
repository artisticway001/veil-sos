// =====================================================================
// VEIL KEYCHAIN - FINAL  (ESP32-C3 SuperMini, latch on GPIO4)
// =====================================================================
// Board: ESP32C3 Dev Module, USB CDC On Boot = Enabled
// Press button -> board powers on -> holds its own power (GPIO4 HIGH)
// -> sends SOS 3 times over ~1.5 s (so the bracelet / basement can't miss it)
// -> lets go of power -> OFF.   On battery: hold the button ~1 s.
// On USB it stays on: press RST to send again.
// =====================================================================
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define LATCH_PIN 4
#define CHANNEL   1
#define MY_ID     1      // 1 = keychain

typedef struct { uint8_t magic, type, id, count; } Msg;
uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  pinMode(LATCH_PIN, OUTPUT); digitalWrite(LATCH_PIN, HIGH);   // hold power FIRST
  Serial.begin(115200); delay(300);
  Serial.println("\nVEIL keychain ON");

  WiFi.mode(WIFI_STA); WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW FAILED");
  } else {
    esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = CHANNEL; p.encrypt = false;
    esp_now_add_peer(&p);
    uint8_t n = 0;
    for (int burst = 1; burst <= 3; burst++) {
      int ok = 0;
      for (int i = 0; i < 8; i++) {
        Msg m = {'V', 1, MY_ID, n++};
        if (esp_now_send(BCAST, (uint8_t *)&m, sizeof(m)) == ESP_OK) ok++;
        delay(40);
      }
      Serial.printf("SOS burst %d sent (%d/8)\n", burst, ok);
      delay(250);
    }
  }

  delay(200);
  Serial.println("Done -> power OFF (stays on if USB is plugged in; press RST to send again)");
  Serial.flush();
  digitalWrite(LATCH_PIN, LOW);
  while (true) delay(1000);   // not deep sleep: keeps the C3 USB port alive
}

void loop() {}

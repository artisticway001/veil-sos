// VEIL GUARD DESK - LED ONLY (no buzzer, no LCD)   Board: Arduino Leonardo
// LED:    D13 -> 220R -> LED long leg, short leg -> GND
// Button: D7 -> button -> GND
// ESP:    ESP GPIO17 -> D0 (RX)    D10 -> 1k -> ESP GPIO27    5V -> ESP VIN    GND -> ESP GND
//
// SOS from basement    -> LED blinks
// Guard presses button -> LED stops + HELP COMING sent to bracelet (bracelet buzzes 3x)
// Serial Monitor 9600 tests:  e = LED 1 s   s = fake SOS   i = status

const int LED_PIN = 13, GUARD_BTN = 7, ACK_OUT = 10;
#define esp Serial1               // RX = D0 (from ESP GPIO17)

bool sosOn = false;
int sosId = 0;
unsigned long lastMsg = 0, ackAt = 0;
String line = "", lastLine = "";

String who(int id) { return id == 1 ? "KEYCHAIN" : id == 2 ? "BRACELET" : "TEST"; }

void ackPulse() {                 // pull ESP GPIO27 LOW 300 ms (never sends 5V)
  digitalWrite(ACK_OUT, LOW); pinMode(ACK_OUT, OUTPUT);
  delay(300);
  pinMode(ACK_OUT, INPUT);
}

void handle(String s) {
  s.trim(); if (s.length() == 0) return;
  lastMsg = millis();
  if (s != lastLine) { Serial.print("[ESP] "); Serial.println(s); lastLine = s; }
  if (s.startsWith("SOS") && !sosOn && millis() - ackAt > 3000) {
    sosOn = true; sosId = s.substring(4).toInt();
    Serial.print("*** SOS from "); Serial.print(who(sosId)); Serial.println(" - press button to answer ***");
  }
  if (s == "IDLE" && sosOn && sosId != 9) { sosOn = false; digitalWrite(LED_PIN, LOW); Serial.println("SOS cleared by ESP"); }
}

void setup() {
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  pinMode(GUARD_BTN, INPUT_PULLUP);
  pinMode(ACK_OUT, INPUT);
  Serial.begin(9600);
  unsigned long t0 = millis(); while (!Serial && millis() - t0 < 2000);
  esp.begin(9600);
  Serial.println("VEIL desk (LED only) ready. Tests: e s i");
}

void loop() {
  // 1. messages from ESP
  while (esp.available()) {
    char c = esp.read();
    if (c == '\n') { handle(line); line = ""; }
    else if (c != '\r' && line.length() < 20) line += c;
  }

  // 2. guard button
  static bool lastBtn = HIGH; static unsigned long btnT = 0;
  bool btn = digitalRead(GUARD_BTN);
  if (btn != lastBtn && millis() - btnT > 50) {
    btnT = millis(); lastBtn = btn;
    if (btn == LOW) {
      if (sosOn) {
        sosOn = false; ackAt = millis();
        digitalWrite(LED_PIN, LOW);                   // LED stops
        ackPulse();                                   // tell ESP -> HELP COMING to bracelet
        Serial.println("Guard answered -> LED off, HELP COMING sent");
      } else Serial.println("[BUTTON] pressed (no SOS active - button works)");
    }
  }

  // 3. tests
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'e') { Serial.println("LED ON 1 s"); digitalWrite(LED_PIN, HIGH); delay(1000); digitalWrite(LED_PIN, LOW); }
    else if (c == 's') { sosOn = true; sosId = 9; Serial.println("Fake SOS - LED blinking, press button"); }
    else if (c == 'i') {
      Serial.print("ESP link: ");
      if (lastMsg == 0) Serial.println("NEVER heard ESP (check GPIO17 -> D0, GND)");
      else { Serial.print("last msg "); Serial.print((millis() - lastMsg) / 1000.0); Serial.println(" s ago"); }
      Serial.print("SOS: "); Serial.println(sosOn ? "ACTIVE" : "none");
      Serial.print("Button: "); Serial.println(digitalRead(GUARD_BTN) == LOW ? "PRESSED" : "up");
    }
  }

  // 4. LED blinks while SOS is active
  if (sosOn) digitalWrite(LED_PIN, (millis() / 250) % 2);
}

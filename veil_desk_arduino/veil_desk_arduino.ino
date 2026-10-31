// VEIL GUARD DESK  (Arduino LEONARDO + 16x2 LCD)  - based on friend's code
// Board: Tools -> Board -> Arduino Leonardo
// LCD: 12, 11, 5, 4, 3, 2 (same as before)   Buzzer: D8   Button: D7 -> GND   LED: D13 -> 220R -> LED -> GND
// From ESP:  ESP GPIO17 -> D0 (RX)   (ESP sends IDLE / SOS 1 / SOS 2 / ACK)
// To ESP:    D10 -> 1k -> ESP GPIO27   (we only pull it LOW, never 5V -> safe for ESP)
// Power:     power bank -> Arduino USB.  Arduino 5V -> ESP VIN,  GND -> ESP GND
//
// SERIAL MONITOR TESTS (9600 baud, laptop USB instead of power bank):
//   i = show all inputs     b = buzzer 1 s     e = LED 1 s     l = LCD test
//   s = fake SOS on desk    a = send ACK pulse to ESP (tests the D10 wire)
//   Every message from the ESP is printed as [ESP] ...  and every button press as [BUTTON]
#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
const int BUZZER = 8, BUTTON = 7, ACK_OUT = 10, LED = 13;
#define esp Serial1           // Leonardo hardware serial: RX = D0 (from ESP GPIO17). D1 not connected

int mode = 0;                 // 0 = ready, 1 = SOS, 2 = help sent
int sosId = 0;
unsigned long lastMsg = 0, helpAt = 0, sosAt = 0;
String line = "", lastLine = "none";
String lcdA = "", lcdB = "";

void show(const char *a, const char *b) {
  if (lcdA == a && lcdB == b) return;
  lcdA = a; lcdB = b;
  lcd.clear(); lcd.print(a); lcd.setCursor(0, 1); lcd.print(b);
  Serial.print("[LCD] "); Serial.print(a); Serial.print(" | "); Serial.println(b);
}

void ackPulse() {                       // pull ESP GPIO27 LOW for 300 ms
  digitalWrite(ACK_OUT, LOW); pinMode(ACK_OUT, OUTPUT);
  delay(300);
  pinMode(ACK_OUT, INPUT);              // let go (no pull-up -> never 5V)
}

void handle(String s) {
  s.trim(); if (s.length() == 0) return;
  lastMsg = millis();
  if (s != lastLine || s != "IDLE") { Serial.print("[ESP] "); Serial.println(s); }
  lastLine = s;
  if (s.startsWith("SOS")) {
    if (mode != 1) { mode = 1; sosId = s.substring(4).toInt(); sosAt = millis(); Serial.println("*** SOS ALERT ***"); }
  } else if (s == "ACK") {
    mode = 2; helpAt = millis();
  } else if (s == "IDLE" && mode == 1 && millis() - sosAt > 3000) {
    mode = 0;                           // ESP cleared it (e.g. ESP BOOT button)
  }
}

void info() {
  Serial.println("------ INPUTS ------");
  Serial.print("Button D7     : "); Serial.println(digitalRead(BUTTON) == LOW ? "PRESSED" : "up");
  Serial.print("ESP link      : ");
  if (lastMsg == 0) Serial.println("NEVER heard ESP (check ESP GPIO17 -> D0 and GND)");
  else { Serial.print("last msg "); Serial.print((millis() - lastMsg) / 1000.0); Serial.println(" s ago"); }
  Serial.print("Last ESP msg  : "); Serial.println(lastLine);
  Serial.print("Mode          : "); Serial.println(mode == 0 ? "READY" : mode == 1 ? "SOS" : "HELP SENT");
  Serial.println("--------------------");
}

void setup() {
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);
  pinMode(LED, OUTPUT); digitalWrite(LED, LOW);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(ACK_OUT, INPUT);
  Serial.begin(9600);
  unsigned long t0 = millis(); while (!Serial && millis() - t0 < 2000);   // Leonardo USB: wait max 2 s for Serial Monitor
  esp.begin(9600);
  lcd.begin(16, 2);
  show("VEIL SECURITY", "STARTING...");
  digitalWrite(BUZZER, HIGH); delay(150); digitalWrite(BUZZER, LOW);
  Serial.println("VEIL desk ready. Tests: i b e l s a");
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
  bool btn = digitalRead(BUTTON);
  if (btn != lastBtn && millis() - btnT > 50) {
    btnT = millis(); lastBtn = btn;
    if (btn == LOW) {
      Serial.println("[BUTTON] pressed");
      if (mode == 1) {
        Serial.println("Guard answered -> ACK pulse to ESP");
        digitalWrite(BUZZER, LOW);
        ackPulse();
        mode = 2; helpAt = millis();
      } else {
        Serial.println("(no SOS active - button works)");
        digitalWrite(BUZZER, HIGH); delay(80); digitalWrite(BUZZER, LOW);
      }
    } else Serial.println("[BUTTON] released");
  }

  // 3. serial monitor tests
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'i') info();
    else if (c == 'e') { Serial.println("LED ON 1 s"); digitalWrite(LED, HIGH); delay(1000); digitalWrite(LED, LOW); }
    else if (c == 'b') { Serial.println("Buzzer ON 1 s"); digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW); }
    else if (c == 'l') { lcd.clear(); lcd.print("LCD TEST 0123456"); lcd.setCursor(0, 1); lcd.print("ABCDEFGHIJKLMNOP");
                         Serial.println("LCD test - all 32 boxes should show text"); delay(2500); lcdA = ""; }
    else if (c == 's') { Serial.println("Fake SOS"); mode = 1; sosId = 9; sosAt = millis(); }
    else if (c == 'a') { Serial.println("ACK pulse -> ESP should print 'Guard answered' (only if ESP has an SOS)"); ackPulse(); }
  }

  // 4. screen + buzzer
  bool linkOk = lastMsg != 0 && millis() - lastMsg < 4000;
  if (mode == 1) {
    show("!! SOS ALERT !!", sosId == 1 ? "FROM: KEYCHAIN" : sosId == 2 ? "FROM: BRACELET" : "FROM: TEST");
    digitalWrite(BUZZER, (millis() / 250) % 2);
    digitalWrite(LED, (millis() / 250) % 2);          // LED blinks with buzzer
  } else if (mode == 2) {
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, HIGH);                          // LED solid = help sent
    show("HELP IS COMING", "SENT TO VICTIM");
    if (millis() - helpAt > 4000) mode = 0;
  } else {
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, linkOk && (millis() % 3000) < 60);   // tiny blink = link OK
    show("VEIL SECURITY", linkOk ? "SYSTEM READY" : "NO BASEMENT LINK");
  }
}

// VEIL GUARD DESK - I2C LCD   Board: Arduino Leonardo
// Every message shows on the LCD AND in Serial Monitor (9600) - same words.
// Library: "LiquidCrystal I2C" by Frank de Brabander
// LCD: GND->GND, VCC->5V, SDA->D2, SCL->D3     Buzzer: D8     LED: D13 -> 220R     Button: D7 -> GND
// ESP: ESP GPIO17 -> D0 (RX),  D10 -> 1k -> ESP GPIO27,  5V -> ESP VIN,  GND -> ESP GND
//
// TESTS (type in Serial Monitor):  l = LCD   b = buzzer   e = LED   s = fake SOS   i = status   a = ACK wire

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int BUZZER = 8, LED = 13, BUTTON = 7, ACK_OUT = 10;
#define esp Serial1

LiquidCrystal_I2C *lcd = nullptr;
uint8_t lcdAddr = 0;
String shownA = "", shownB = "";

int mode = 0;                 // 0 = ready, 1 = SOS, 2 = help sent
int sosId = 0;
unsigned long lastMsg = 0, helpAt = 0, sosAt = 0, holdUntil = 0;
String line = "";

// ---------- ONE function for LCD + Serial ----------
void say(String a, String b) {
  if (a == shownA && b == shownB) return;          // no repeats
  shownA = a; shownB = b;
  Serial.println("+----------------+");
  Serial.print("|"); Serial.print(a); for (int i = a.length(); i < 16; i++) Serial.print(' '); Serial.println("|");
  Serial.print("|"); Serial.print(b); for (int i = b.length(); i < 16; i++) Serial.print(' '); Serial.println("|");
  Serial.println("+----------------+");
  if (lcd) { lcd->clear(); lcd->print(a); lcd->setCursor(0, 1); lcd->print(b); }
}
// show a message for some ms, then go back to normal screen
void flash(String a, String b, unsigned long ms) { say(a, b); holdUntil = millis() + ms; }

void findLcd() {
  Wire.begin();
  uint8_t tryAddr[] = {0x27, 0x3F, 0x20, 0x38};
  for (uint8_t i = 0; i < 4; i++) {
    Wire.beginTransmission(tryAddr[i]);
    if (Wire.endTransmission() == 0) { lcdAddr = tryAddr[i]; break; }
  }
  if (lcdAddr) { lcd = new LiquidCrystal_I2C(lcdAddr, 16, 2); lcd->init(); lcd->backlight(); }
}

void out(bool on) { digitalWrite(BUZZER, on); digitalWrite(LED, on); }

void ackPulse() {             // pull ESP GPIO27 LOW 300 ms (never 5V)
  digitalWrite(ACK_OUT, LOW); pinMode(ACK_OUT, OUTPUT); delay(300); pinMode(ACK_OUT, INPUT);
}

String who(int id) { return id == 1 ? "KEYCHAIN" : id == 2 ? "BRACELET" : "TEST"; }

void handle(String s) {
  s.trim(); if (s.length() == 0) return;
  lastMsg = millis();
  if (s.startsWith("SOS")) {
    if (mode != 1) { mode = 1; sosId = s.substring(4).toInt(); sosAt = millis(); holdUntil = 0; }
  } else if (s == "ACK") {
    mode = 2; helpAt = millis(); holdUntil = 0;
  } else if (s == "IDLE" && mode == 1 && millis() - sosAt > 3000) {
    mode = 0;
  }
}

void setup() {
  pinMode(BUZZER, OUTPUT); pinMode(LED, OUTPUT); out(false);
  pinMode(BUTTON, INPUT_PULLUP); pinMode(ACK_OUT, INPUT);
  Serial.begin(9600);
  unsigned long t0 = millis(); while (!Serial && millis() - t0 < 2000);
  esp.begin(9600);
  findLcd();
  if (lcdAddr) { String hex = String(lcdAddr, HEX); hex.toUpperCase(); flash("VEIL SECURITY", "LCD OK 0x" + hex, 1500); }
  else Serial.println("!! LCD NOT FOUND - check SDA->D2, SCL->D3, VCC, GND (Serial still works)");
  out(true); delay(150); out(false);
  Serial.println("Tests: l b e s i a");
}

void loop() {
  unsigned long t = millis();

  // 1. messages from ESP
  while (esp.available()) {
    char c = esp.read();
    if (c == '\n') { handle(line); line = ""; }
    else if (c != '\r' && line.length() < 20) line += c;
  }

  // 2. guard button
  static bool lastBtn = HIGH; static unsigned long btnT = 0;
  bool btn = digitalRead(BUTTON);
  if (btn != lastBtn && t - btnT > 50) {
    btnT = t; lastBtn = btn;
    if (btn == LOW) {
      if (mode == 1) { out(false); ackPulse(); mode = 2; helpAt = millis(); holdUntil = 0; }
      else { flash("BUTTON PRESSED", "BUTTON WORKS OK", 1500); out(true); delay(80); out(false); }
    }
  }

  // 3. tests
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'l') {
      flash("LCD TEST 0123456", "ABCDEFGHIJKLMNOP", 2500);
      if (!lcd) Serial.println("(LCD not found - only Serial shows this)");
    }
    else if (c == 'b') { flash("TEST: BUZZER", "ON FOR 1 SEC", 1500); digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW); }
    else if (c == 'e') { flash("TEST: LED", "ON FOR 1 SEC", 1500); digitalWrite(LED, HIGH); delay(1000); digitalWrite(LED, LOW); }
    else if (c == 's') { mode = 1; sosId = 9; sosAt = millis(); holdUntil = 0; }
    else if (c == 'a') { flash("TEST: ACK WIRE", "PULSE TO ESP", 1500); ackPulse(); }
    else if (c == 'i') {
      bool ok = lastMsg != 0 && t - lastMsg < 4000;
      String b = digitalRead(BUTTON) == LOW ? "BTN:DOWN " : "BTN:up ";
      flash(ok ? "ESP LINK: OK" : (lastMsg ? "ESP LINK: LOST" : "ESP LINK: NONE"), b + (lcd ? "LCD:OK" : "LCD:--"), 3000);
    }
  }

  // 4. screen + buzzer + LED
  if (mode == 1) {
    say("!! SOS ALERT !!", "FROM: " + who(sosId));
    if (sosId == 2) { unsigned long p = t % 1000; out(p < 120 || (p > 240 && p < 360)); }   // bracelet: double beep
    else out((t / 200) % 2);                                                                // keychain: fast beeps
  } else if (mode == 2) {
    digitalWrite(BUZZER, LOW); digitalWrite(LED, HIGH);
    say("HELP IS COMING", "SENT TO " + who(sosId));
    if (t - helpAt > 4000) { mode = 0; out(false); }
  } else {
    digitalWrite(BUZZER, LOW);
    bool ok = lastMsg != 0 && t - lastMsg < 4000;
    digitalWrite(LED, ok && (t % 3000) < 60);
    if (t > holdUntil) say("VEIL SECURITY", ok ? "SYSTEM READY" : "NO BASEMENT LINK");
  }
}

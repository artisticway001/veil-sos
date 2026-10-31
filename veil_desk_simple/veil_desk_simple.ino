// VEIL GUARD DESK - SIMPLE (no LCD)   Board: Arduino Leonardo
// Buzzer: D8 -> buzzer + , buzzer - -> GND
// LED:    D13 -> 220R -> LED long leg, short leg -> GND
// Button: D7 -> button -> GND
// ESP:    ESP GPIO17 -> D0 (RX)      D10 -> 1k -> ESP GPIO27
//         Arduino 5V -> ESP VIN      GND -> ESP GND
// Power:  power bank -> Leonardo USB
//
// What happens:
//   SOS from basement -> buzzer + LED beep fast (keychain = fast beeps, bracelet = double beeps)
//   Guard presses button -> HELP COMING sent -> LED stays ON 4 s
//   Idle: tiny LED blink every 3 s = basement ESP connected.  No blink = check the wires.
//
// SERIAL MONITOR TESTS (9600):  b = buzzer 1 s   e = LED 1 s   s = fake SOS   i = inputs   a = ACK pulse to ESP

const int BUZZER = 8, LED = 13, BUTTON = 7, ACK_OUT = 10;
#define esp Serial1               // RX = D0 (from ESP GPIO17)

int mode = 0;                     // 0 = ready, 1 = SOS, 2 = help sent
int sosId = 0;
unsigned long lastMsg = 0, helpAt = 0, sosAt = 0;
String line = "", lastLine = "none";

void out(bool on) { digitalWrite(BUZZER, on); digitalWrite(LED, on); }

void ackPulse() {                 // pull ESP GPIO27 LOW for 300 ms (never sends 5V)
  digitalWrite(ACK_OUT, LOW); pinMode(ACK_OUT, OUTPUT);
  delay(300);
  pinMode(ACK_OUT, INPUT);
}

void handle(String s) {
  s.trim(); if (s.length() == 0) return;
  lastMsg = millis();
  if (s != lastLine) { Serial.print("[ESP] "); Serial.println(s); }   // print only changes
  lastLine = s;
  if (s.startsWith("SOS")) {
    if (mode != 1) {
      mode = 1; sosId = s.substring(4).toInt(); sosAt = millis();
      Serial.print("*** SOS ALERT from "); Serial.println(sosId == 1 ? "KEYCHAIN" : sosId == 2 ? "BRACELET" : "TEST");
    }
  } else if (s == "ACK") { mode = 2; helpAt = millis(); Serial.println("HELP COMING sent to victim"); }
  else if (s == "IDLE" && mode == 1 && millis() - sosAt > 3000) { mode = 0; Serial.println("SOS cleared by ESP"); }
}

void info() {
  Serial.println("------ INPUTS ------");
  Serial.print("Button D7 : "); Serial.println(digitalRead(BUTTON) == LOW ? "PRESSED" : "up");
  Serial.print("ESP link  : ");
  if (lastMsg == 0) Serial.println("NEVER heard ESP (check GPIO17 -> D0 and GND)");
  else { Serial.print("last msg "); Serial.print((millis() - lastMsg) / 1000.0); Serial.println(" s ago"); }
  Serial.print("Last msg  : "); Serial.println(lastLine);
  Serial.print("Mode      : "); Serial.println(mode == 0 ? "READY" : mode == 1 ? "SOS" : "HELP SENT");
  Serial.println("--------------------");
}

void setup() {
  pinMode(BUZZER, OUTPUT); pinMode(LED, OUTPUT); out(false);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(ACK_OUT, INPUT);
  Serial.begin(9600);
  unsigned long t0 = millis(); while (!Serial && millis() - t0 < 2000);   // Leonardo: wait max 2 s for monitor
  esp.begin(9600);
  out(true); delay(150); out(false);
  Serial.println("VEIL desk (simple) ready. Tests: b e s i a");
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
      if (mode == 1) { Serial.println("Guard answered -> ACK to ESP"); out(false); ackPulse(); mode = 2; helpAt = millis(); }
      else { Serial.println("(no SOS active - button works)"); out(true); delay(80); out(false); }
    } else Serial.println("[BUTTON] released");
  }

  // 3. serial monitor tests
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'i') info();
    else if (c == 'b') { Serial.println("Buzzer ON 1 s"); digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW); }
    else if (c == 'e') { Serial.println("LED ON 1 s"); digitalWrite(LED, HIGH); delay(1000); digitalWrite(LED, LOW); }
    else if (c == 's') { Serial.println("Fake SOS - press the button to answer"); mode = 1; sosId = 9; sosAt = millis(); }
    else if (c == 'a') { Serial.println("ACK pulse -> ESP should print 'Guard answered' (only if ESP has an SOS)"); ackPulse(); }
  }

  // 4. buzzer + LED
  unsigned long t = millis();
  if (mode == 1) {
    if (sosId == 2) { unsigned long p = t % 1000; out(p < 120 || (p > 240 && p < 360)); }   // bracelet: double beep
    else out((t / 200) % 2);                                                                // keychain/test: fast beeps
  } else if (mode == 2) {
    digitalWrite(BUZZER, LOW); digitalWrite(LED, HIGH);
    if (t - helpAt > 4000) { mode = 0; out(false); }
  } else {
    digitalWrite(BUZZER, LOW);
    bool linkOk = lastMsg != 0 && t - lastMsg < 4000;
    digitalWrite(LED, linkOk && (t % 3000) < 60);
  }
}

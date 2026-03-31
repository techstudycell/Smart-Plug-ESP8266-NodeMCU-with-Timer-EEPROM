/**********************************************************************************
 *  TITLE: Webserver AP control 2 Relays using NodeMCU with Timer (Real time feedback  + EEPROM)
 *  Click on the following links to learn more. 
 *  YouTube Video: https://youtu.be/TxwKXW1aC0o
 *  Related Blog : https://iotcircuithub.com/esp8266-projects/
 *  by Tech StudyCell
 *  Preferences--> Aditional boards Manager URLs : 
 *  https://dl.espressif.com/dl/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  
 *  Download Board ESP8266 (3.1.2) : https://github.com/esp8266/Arduino
 *
 *  Download the libraries 
 *
 **********************************************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// ================= USER SETTINGS =================
#define USE_LATCHED_SWITCH  1  // 1 = Latched, 0 = Push Button
#define USE_EEPROM 1             // 1 = Enable EEPROM, 0 = Disable EEPROM
#define RELAY_ACTIVE_LOW 1     // 1 = Active LOW relay, 0 = Active HIGH

// AP Credentials
const char* ssid = "Smart_Plug";
const char* password = "12345678";

// Pins
#define RELAY1 D5
#define RELAY2 D6
#define SW1 D1
#define SW2 D2
#define LED D0   // GPIO16

ESP8266WebServer server(80);

// States
bool relay1State = false;
bool relay2State = false;

// Timer
bool timerActive = false;
unsigned long timerStart = 0;
unsigned long timerDuration = 0;

// Switch memory
bool lastSW1 = HIGH;
bool lastSW2 = HIGH;

// ================= RELAY CONTROL =================
void setRelay(int pin, bool state) {
#if RELAY_ACTIVE_LOW
  digitalWrite(pin, state ? LOW : HIGH);
#else
  digitalWrite(pin, state ? HIGH : LOW);
#endif
}

// ================= EEPROM =================
void saveState() {
#if USE_EEPROM
  EEPROM.write(0, relay1State);
  EEPROM.write(1, relay2State);
  EEPROM.commit();
  Serial.println("💾 State Saved");
#endif
}

void loadState() {
#if USE_EEPROM
  relay1State = EEPROM.read(0);
  relay2State = EEPROM.read(1);
  Serial.println("💾 State Loaded");
#endif
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

#if USE_EEPROM
  EEPROM.begin(512);
#endif

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  loadState();

  setRelay(RELAY1, relay1State);
  setRelay(RELAY2, relay2State);
  digitalWrite(LED, LOW);

  WiFi.softAP(ssid, password);

  Serial.println("\n===== ESP8266 AP MODE =====");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/toggle1", toggleRelay1);
  server.on("/toggle2", toggleRelay2);
  server.on("/status", sendStatus);
  server.on("/setTimer", setTimer);
  server.on("/stopTimer", stopTimer);

  server.begin();
}

// ================= LOOP =================
void loop() {
  server.handleClient();
  checkSwitches();
  checkTimer();
}

// ================= SWITCH =================
void checkSwitches() {
  bool sw1 = digitalRead(SW1);
  bool sw2 = digitalRead(SW2);

#if USE_LATCHED_SWITCH
  if (sw1 != lastSW1) {
    relay1State = (sw1 == LOW);
    setRelay(RELAY1, relay1State);
    Serial.println("Latched SW1 Changed");
    saveState();
  }

  if (sw2 != lastSW2) {
    relay2State = (sw2 == LOW);
    setRelay(RELAY2, relay2State);
    Serial.println("Latched SW2 Changed");
    saveState();
  }
#else
  if (sw1 == LOW && lastSW1 == HIGH) {
    relay1State = !relay1State;
    setRelay(RELAY1, relay1State);
    Serial.println("Push SW1 Pressed");
    saveState();
  }

  if (sw2 == LOW && lastSW2 == HIGH) {
    relay2State = !relay2State;
    setRelay(RELAY2, relay2State);
    Serial.println("Push SW2 Pressed");
    saveState();
  }
#endif

  lastSW1 = sw1;
  lastSW2 = sw2;
}

// ================= TIMER =================
void checkTimer() {
  if (timerActive) {
    if (millis() - timerStart >= timerDuration) {
      relay2State = false;
      setRelay(RELAY2, relay2State);
      timerActive = false;

      digitalWrite(LED, LOW);

      Serial.println("⏱ Timer Finished");
      saveState();
    }
  }
}

// ================= WEB =================
void handleRoot() {
  String html = R"====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; background:#0f172a; color:white; text-align:center; }
.card { background:#1e293b; padding:20px; margin:12px; border-radius:18px; box-shadow:0 0 10px #000;}
button {
  padding:15px;
  font-size:18px;
  border:none;
  border-radius:12px;
  width:130px;
  margin:5px;
  color:white;
}
.on { background: linear-gradient(45deg,#22c55e,#16a34a); }
.off { background: linear-gradient(45deg,#ef4444,#dc2626); }
.timer { background: linear-gradient(45deg,#3b82f6,#2563eb); }
.stop { background: linear-gradient(45deg,#f97316,#ea580c); }

.btn-row {
  display:flex;
  justify-content:center;
  gap:10px;
  flex-wrap:wrap;
}

input { padding:10px; font-size:16px; width:80px; border-radius:8px; border:none;}
.footer { margin-top:20px; font-size:14px; opacity:0.7;}
</style>
</head>

<body>

<h2>ESP8266 Smart Plug</h2>

<div class="card">
<h3>Relay 1</h3>
<button onclick="toggle(1)" id="r1">--</button>
</div>

<div class="card">
<h3>Relay 2</h3>
<button onclick="toggle(2)" id="r2">--</button>
</div>

<div class="card">
<h3>Timer (Relay 2)</h3>

<input id="time" type="number" placeholder="Time">
<br><br>

<label><input type="radio" name="unit" value="min" checked> Min</label>
<label><input type="radio" name="unit" value="hr"> Hr</label>

<br><br>

<div class="btn-row">
<button class="timer" onclick="setTimer()">Start</button>
<button class="stop" onclick="stopTimer()">Stop</button>
</div>

<h3 id="countdown">--</h3>

</div>

<div class="footer">Created by Tech StudyCell</div>

<script>
function toggle(id){
  fetch('/toggle'+id);
}

function setTimer(){
  let t = document.getElementById("time").value;
  let unit = document.querySelector('input[name="unit"]:checked').value;
  fetch(`/setTimer?time=${t}&unit=${unit}`);
}

function stopTimer(){
  fetch('/stopTimer');
}

function formatTime(sec){
  if(sec < 60) return sec + " sec";

  let m = Math.floor(sec/60);
  let s = sec % 60;

  return m + " min " + s + " sec";
}

function update(){
  fetch('/status')
  .then(res=>res.json())
  .then(data=>{
    let r1 = document.getElementById("r1");
    let r2 = document.getElementById("r2");

    r1.innerHTML = data.r1 ? "ON" : "OFF";
    r1.className = data.r1 ? "on" : "off";

    r2.innerHTML = data.r2 ? "ON" : "OFF";
    r2.className = data.r2 ? "on" : "off";

    if(data.timer){
      document.getElementById("countdown").innerHTML = formatTime(data.remaining);
    } else {
      document.getElementById("countdown").innerHTML = "Timer OFF";
    }
  });
}

setInterval(update, 1000);
</script>

</body>
</html>
)====";

  server.send(200, "text/html", html);
}

// ================= API =================
void toggleRelay1() {
  relay1State = !relay1State;
  setRelay(RELAY1, relay1State);
  Serial.println("Relay1 Toggled (Web)");
  saveState();
  server.send(200, "text/plain", "OK");
}

void toggleRelay2() {
  relay2State = !relay2State;
  setRelay(RELAY2, relay2State);
  Serial.println("Relay2 Toggled (Web)");
  saveState();
  server.send(200, "text/plain", "OK");
}

void sendStatus() {
  String json = "{";

  json += "\"r1\":" + String(relay1State);
  json += ",\"r2\":" + String(relay2State);
  json += ",\"timer\":" + String(timerActive);

  if (timerActive) {
    unsigned long remaining = (timerDuration - (millis() - timerStart)) / 1000;
    json += ",\"remaining\":" + String(remaining);
  } else {
    json += ",\"remaining\":0";
  }

  json += "}";

  server.send(200, "application/json", json);
}

// ================= TIMER =================
void setTimer() {
  if (server.hasArg("time") && server.hasArg("unit")) {
    int t = server.arg("time").toInt();
    String unit = server.arg("unit");

    if (unit == "min") timerDuration = t * 60000UL;
    else timerDuration = t * 3600000UL;

    timerStart = millis();
    timerActive = true;

    relay2State = true;
    setRelay(RELAY2, relay2State);

    digitalWrite(LED, HIGH);

    Serial.println("⏱ Timer Started");
    saveState();
  }
  server.send(200, "text/plain", "Timer Set");
}

void stopTimer() {
  timerActive = false;
  digitalWrite(LED, LOW);

  Serial.println("⛔ Timer Stopped");

  server.send(200, "text/plain", "Stopped");
}
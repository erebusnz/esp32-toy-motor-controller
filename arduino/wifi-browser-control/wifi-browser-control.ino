// ESP32 Toy Motor Controller - WiFi Browser Control with OTA
// Hardware: ESP32S3-Super Mini + TB6612FNG dual H-bridge motor driver
//
// IMPORTANT - Arduino IDE setup:
//   Board:     ESP32S3 Dev Module
//   Core:      Espressif ESP32 Arduino core v3.x or later (required for ledcAttach)
//   Upload speed: 921600, USB CDC On Boot: Enabled

#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include "secrets.h"

uint32_t last_ota_time = 0;

// --- Motor A Pins (TB6612FNG) - Left motor ---
#define PWMA  8
#define AIN1  10
#define AIN2  9

// --- Motor B Pins (TB6612FNG) - Right motor ---
#define PWMB  13
#define BIN1  11
#define BIN2  12

// --- TB6612FNG Standby (HIGH = enabled, LOW = standby) ---
#define STBY  2

// --- Switch Outputs (NPN low-side: HIGH = ON, LOW = OFF) ---
#define SW1   1
#define SW2   7

// --- Onboard LED ---
#define LED   48

// --- PWM Settings ---
#define PWM_FREQ        5000
#define PWM_RESOLUTION  8     // 0-255

// TB6612FNG direction truth table:
//   AIN1=H, AIN2=L -> Forward
//   AIN1=L, AIN2=H -> Backward
//   AIN1=L, AIN2=L -> Coast (stop)

bool sw1_state = false;
bool sw2_state = false;
uint32_t sw1_off_at = 0;  // millis() timestamp to auto-off SW1 (0 = not scheduled)

WebServer server(80);

// ----- Motor helpers -----

void motorA_forward(uint8_t speed) {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH);  // inverted: motor A wired in reverse
  ledcWrite(PWMA, speed);
}
void motorA_backward(uint8_t speed) {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);  // inverted: motor A wired in reverse
  ledcWrite(PWMA, speed);
}
void motorA_stop() {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);
  ledcWrite(PWMA, 0);
}

void motorB_forward(uint8_t speed) {
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  ledcWrite(PWMB, speed);
}
void motorB_backward(uint8_t speed) {
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH);
  ledcWrite(PWMB, speed);
}
void motorB_stop() {
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
  ledcWrite(PWMB, 0);
}

// ----- Web page -----

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Robot Controller</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #1a1a2e; color: #eee; font-family: sans-serif;
    display: flex; flex-direction: column; align-items: center;
    min-height: 100vh; padding: 24px 16px; gap: 24px;
  }
  h1 { font-size: 1.4em; letter-spacing: 0.05em; }
  .dpad {
    display: grid;
    grid-template-columns: repeat(3, 90px);
    grid-template-rows: repeat(3, 90px);
    gap: 10px;
  }
  .btn {
    background: #16213e; border: 2px solid #0f3460;
    border-radius: 14px; color: #e94560; font-size: 2em;
    cursor: pointer; user-select: none; touch-action: none;
    display: flex; align-items: center; justify-content: center;
    -webkit-tap-highlight-color: transparent;
    transition: background 0.08s, border-color 0.08s;
  }
  .btn:active, .btn.pressed { background: #0f3460; border-color: #e94560; }
  .empty { visibility: hidden; }
  .speed-row {
    display: flex; flex-direction: column; align-items: center; gap: 6px;
    width: 280px;
  }
  .speed-row label { font-size: 0.9em; color: #aaa; }
  input[type=range] { width: 100%; accent-color: #e94560; }
  .switches { display: flex; gap: 16px; }
  .sw-btn {
    width: 120px; height: 64px; border-radius: 14px;
    font-size: 1em; font-weight: bold;
    border: 2px solid #0f3460; background: #16213e; color: #666;
    cursor: pointer; touch-action: manipulation;
    -webkit-tap-highlight-color: transparent;
    transition: background 0.1s, border-color 0.1s, color 0.1s;
  }
  .sw-btn.on  { background: #1a3a1a; border-color: #4caf50; color: #4caf50; }
  .sw-btn.firing { background: #3a1a1a; border-color: #e94560; color: #e94560; }
  #status { font-size: 0.8em; color: #555; min-height: 1.2em; }
</style>
</head>
<body>
<h1>Robot Controller</h1>

<div class="dpad">
  <div class="empty"></div>
  <button class="btn" id="fwd">&#x25B2;</button>
  <div class="empty"></div>

  <button class="btn" id="left">&#x25C4;</button>
  <button class="btn" id="stop_btn" style="font-size:1.2em;">&#x25A0;</button>
  <button class="btn" id="right">&#x25BA;</button>

  <div class="empty"></div>
  <button class="btn" id="back">&#x25BC;</button>
  <div class="empty"></div>
</div>

<div class="speed-row">
  <label>Speed: <span id="spd_lbl">180</span></label>
  <input type="range" id="speed" min="50" max="255" value="180"
    oninput="document.getElementById('spd_lbl').textContent=this.value">
</div>

<div class="switches">
  <button class="sw-btn" id="sw1_btn" onclick="cmd('sw1')">Shoot<br><small>READY</small></button>
  <button class="sw-btn" id="sw2_btn" onclick="cmd('sw2')">Headlight<br><small>OFF</small></button>
</div>

<div id="status"></div>

<script>
function speed() { return document.getElementById('speed').value; }

function cmd(action) {
  fetch('/cmd?action=' + action + '&speed=' + speed())
    .then(r => r.json())
    .then(s => {
      const sw1 = document.getElementById('sw1_btn');
      const sw2 = document.getElementById('sw2_btn');
      sw1.className = 'sw-btn' + (s.sw1 ? ' firing' : '');
      sw1.innerHTML = 'Shoot<br><small>' + (s.sw1 ? 'FIRING' : 'READY') + '</small>';
      sw2.className = 'sw-btn' + (s.sw2 ? ' on' : '');
      sw2.innerHTML = 'Headlight<br><small>' + (s.sw2 ? 'ON' : 'OFF') + '</small>';
    })
    .catch(() => { document.getElementById('status').textContent = 'Connection lost'; });
}

// Hold-to-move buttons: pointerdown = move, pointerup/leave = stop
const moveMap = { fwd: 'forward', left: 'left', right: 'right', back: 'backward' };
Object.entries(moveMap).forEach(([id, action]) => {
  const el = document.getElementById(id);
  el.addEventListener('pointerdown', e => { e.preventDefault(); el.classList.add('pressed'); cmd(action); });
  el.addEventListener('pointerup',   e => { el.classList.remove('pressed'); cmd('stop'); });
  el.addEventListener('pointerleave',e => { el.classList.remove('pressed'); cmd('stop'); });
});

document.getElementById('stop_btn').addEventListener('click', () => cmd('stop'));
</script>
</body>
</html>
)rawliteral";

// ----- HTTP handlers -----

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleCmd() {
  if (!server.hasArg("action")) {
    server.send(400, "text/plain", "Missing action");
    return;
  }

  String action = server.arg("action");
  uint8_t spd = 180;
  if (server.hasArg("speed")) {
    spd = (uint8_t)constrain(server.arg("speed").toInt(), 0, 255);
  }

  if      (action == "forward")  { motorA_forward(spd);  motorB_forward(spd);  }
  else if (action == "backward") { motorA_backward(spd); motorB_backward(spd); }
  else if (action == "left")     { motorA_forward(spd);  motorB_backward(spd); }  // pivot left
  else if (action == "right")    { motorA_backward(spd); motorB_forward(spd);  }  // pivot right
  else if (action == "stop")     { motorA_stop(); motorB_stop(); }
  else if (action == "sw1")      { sw1_state = true; digitalWrite(SW1, HIGH); sw1_off_at = millis() + 5000; }
  else if (action == "sw2")      { sw2_state = !sw2_state; digitalWrite(SW2, sw2_state); }

  String json = "{\"sw1\":" + String(sw1_state ? "true" : "false") +
                ",\"sw2\":" + String(sw2_state ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// ----- Setup -----

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_max_tx_power(34);  // 8.5 dBm — needed for Super Mini on UniFi
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection failed, rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA
    .onStart([]() {
      motorA_stop(); motorB_stop();  // safe state before flashing
      String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
      Serial.println("OTA start: " + type);
    })
    .onEnd([]()   { Serial.println("\nOTA end"); })
    .onProgress([](unsigned int progress, unsigned int total) {
      if (millis() - last_ota_time > 500) {
        Serial.printf("OTA: %u%%\n", progress / (total / 100));
        last_ota_time = millis();
      }
    })
    .onError([](ota_error_t error) {
      const char* msg = "Unknown";
      if      (error == OTA_AUTH_ERROR)    msg = "Auth failed";
      else if (error == OTA_BEGIN_ERROR)   msg = "Begin failed";
      else if (error == OTA_CONNECT_ERROR) msg = "Connect failed";
      else if (error == OTA_RECEIVE_ERROR) msg = "Receive failed";
      else if (error == OTA_END_ERROR)     msg = "End failed";
      Serial.printf("OTA error[%u]: %s\n", error, msg);
    });

  ArduinoOTA.begin();

  server.on("/",    handleRoot);
  server.on("/cmd", handleCmd);
  server.begin();

  Serial.print("Ready — http://");
  Serial.println(WiFi.localIP());

  // --- GPIO setup ---
  pinMode(STBY, OUTPUT); digitalWrite(STBY, HIGH);
  pinMode(LED,  OUTPUT);
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(SW1,  OUTPUT); digitalWrite(SW1, LOW);
  pinMode(SW2,  OUTPUT); digitalWrite(SW2, LOW);

  ledcAttach(PWMA, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PWMB, PWM_FREQ, PWM_RESOLUTION);

  motorA_stop();
  motorB_stop();

  Serial.println("Motor controller ready.");
}

// ----- Loop -----

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  // Auto-off SW1 (Shoot) after 5s
  if (sw1_off_at && millis() >= sw1_off_at) {
    sw1_state = false;
    digitalWrite(SW1, LOW);
    sw1_off_at = 0;
  }
}

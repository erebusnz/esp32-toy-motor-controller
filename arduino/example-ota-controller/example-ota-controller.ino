// ESP32 Toy Motor Controller - Hardware Test Sketch with OTA
// Confirms that motor LEFT, motor RIGHT, Switch 1, and Switch 2 are working correctly.
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
#include "secrets.h"

uint32_t last_ota_time = 0;

// --- Motor A Pins (TB6612FNG) ---
#define PWMA  8   // Motor A PWM speed
#define AIN1  10  // Motor A direction control 1
#define AIN2  9   // Motor A direction control 2

// --- Motor B Pins (TB6612FNG) ---
#define PWMB  13  // Motor B PWM speed
#define BIN1  11  // Motor B direction control 1
#define BIN2  12  // Motor B direction control 2

// --- TB6612FNG Standby (HIGH = motor driver enabled, LOW = standby/disabled) ---
#define STBY  2

// --- Switch Outputs (NPN low-side: GPIO HIGH = switch ON, GPIO LOW = switch OFF) ---
#define SW1   1
#define SW2   7

// --- Onboard LED ---
#define LED   48

// --- PWM Settings ---
#define PWM_FREQ        5000  // Hz
#define PWM_RESOLUTION  8     // bits (duty cycle range: 0-255)
#define MOTOR_SPEED     180   // 0-255

// TB6612FNG direction truth table:
//   AIN1=H, AIN2=L -> Forward
//   AIN1=L, AIN2=H -> Backward
//   AIN1=L, AIN2=L -> Coast (stop)
//   AIN1=H, AIN2=H -> Brake
//
// STBY is driven HIGH in setup() via GP2 — motor driver enabled at startup.

void motorA_forward(uint8_t speed) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  ledcWrite(PWMA, speed);
}

void motorA_backward(uint8_t speed) {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  ledcWrite(PWMA, speed);
}

void motorA_stop() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  ledcWrite(PWMA, 0);
}

void motorB_forward(uint8_t speed) {
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  ledcWrite(PWMB, speed);
}

void motorB_backward(uint8_t speed) {
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  ledcWrite(PWMB, speed);
}

void motorB_stop() {
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  ledcWrite(PWMB, 0);
}

void setup() {
  Serial.begin(115200);

  // --- WiFi + OTA setup ---
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  esp_wifi_set_max_tx_power(34); // 8.5 dBm supermini hardware issue
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {
        type = "filesystem";
      }
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      if (millis() - last_ota_time > 500) {
        Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
        last_ota_time = millis();
      }
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // --- Motor / switch setup ---
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);  // enable motor driver immediately

  pinMode(LED,  OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(SW1,  OUTPUT);
  pinMode(SW2,  OUTPUT);

  // Switches default OFF (NPN: GPIO LOW = transistor off = switch OFF)
  digitalWrite(SW1, LOW);
  digitalWrite(SW2, LOW);

  // Attach PWM to motor speed pins — requires ESP32 Arduino core v3+
  ledcAttach(PWMA, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PWMB, PWM_FREQ, PWM_RESOLUTION);

  motorA_stop();
  motorB_stop();

  Serial.println("Motor controller ready.");
}

void loop() {
  ArduinoOTA.handle();

  // --- Forward ---
  Serial.println("Motor A + B: FORWARD  | SW1 ON, SW2 OFF");
  digitalWrite(LED, HIGH);
  digitalWrite(SW1, HIGH);   // SW1 ON
  digitalWrite(SW2, LOW);    // SW2 OFF
  motorA_forward(MOTOR_SPEED);
  motorB_forward(MOTOR_SPEED);
  delay(2000);

  // --- Brief stop between directions ---
  motorA_stop();
  motorB_stop();
  digitalWrite(SW1, LOW);    // both switches OFF during stop
  digitalWrite(SW2, LOW);
  delay(400);

  // --- Backward ---
  Serial.println("Motor A + B: BACKWARD | SW1 OFF, SW2 ON");
  digitalWrite(LED, LOW);
  digitalWrite(SW1, LOW);    // SW1 OFF
  digitalWrite(SW2, HIGH);   // SW2 ON
  motorA_backward(MOTOR_SPEED);
  motorB_backward(MOTOR_SPEED);
  delay(2000);

  // --- Brief stop before repeating ---
  motorA_stop();
  motorB_stop();
  digitalWrite(SW1, LOW);    // both switches OFF during stop
  digitalWrite(SW2, LOW);
  delay(400);
}

//------------------------- motor 1 -----------------------//
#define BLYNK_TEMPLATE_ID "TMPL30KN8p-M5"
#define BLYNK_TEMPLATE_NAME "Motor Monitor"
#define BLYNK_AUTH_TOKEN "YP0_24KyBn_ByAe62G25mXS2dVSlF2kt"

// //------------------------- motor 2 -----------------------//
// #define BLYNK_TEMPLATE_ID "TMPL3C3KlLDOp"
// #define BLYNK_TEMPLATE_NAME "Motor Monitor 2"
// #define BLYNK_AUTH_TOKEN "Q6sfh2F9NYiKR4rkAHOveqAwMC0XspJJ"

// //------------------------- motor 3 -----------------------//

// #define BLYNK_TEMPLATE_ID "TMPL3hvIg1F3r"
// #define BLYNK_TEMPLATE_NAME "Motor Monitor 3"
// #define BLYNK_AUTH_TOKEN "-vzKutEbtvY7D8IWi2RbKL5G4dxU8R1q"

#include <device_config.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <pzem.h>
#include <EEPROM.h>
#include <Wire.h>
#include "time.h"
#include <ESP_Google_Sheet_Client.h>
#include <GS_SDHelper.h>
#include <esp_task_wdt.h>
#include <ESP32Ping.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "WILD HONEY-2.4G";
char pass[] = "arunprasath@88";

BlynkTimer timer;

// Runtime variables
unsigned long totalRuntimeMinutes = 0;
unsigned long lastRuntimeSave = 0;
const unsigned long runtimeSaveInterval = 60000; // Save every 1 minute
bool runtimeInitialized = false;

// RPM calculation variables
volatile unsigned long rotationCount = 0;
unsigned long lastRPMCalculation = 0;
float RPM = 0;
const int hallSensorPin = 25;
const int magnets = 1;

// Motor state
bool motorRunning = false;
unsigned long motorStartTime = 0;

// Alert flags
bool lowBatteryAlertSent = false;
bool motorStoppedAlertSent = false;

// EEPROM addresses
const int eepromRuntimeAddr = 0;

// NTP server
const char *ntpServer = "pool.ntp.org";
String current_time;

// Connection management
unsigned long lastBlynkConnectionAttempt = 0;
const unsigned long blynkReconnectInterval = 30000;
bool blynkConnected = false;

// Serial print
unsigned long lastSerialPrint = 0;
const unsigned long serialPrintInterval = 5000;

// Google Sheets
const unsigned long sheetUpdateInterval = 600000;
const unsigned long initialSheetUpdateDelay = 60000;
const int MAX_SHEET_RETRIES = 3;

// Internet monitoring
unsigned long lastInternetCheck = 0;
const unsigned long internetCheckInterval = 10000; // Check every 10 seconds
unsigned long internetUnavailableSince = 0;
const unsigned long internetUnavailableTimeout = 60000; // Restart after 1 minute

// Time synchronization
const int TIME_SYNC_RETRIES = 5;
const int TIME_SYNC_RETRY_DELAY = 2000; // 2 seconds between retries
bool timeSyncSuccess = false;

bool alert = true;

// Watchdog timer
const int WDT_TIMEOUT = 30; // 30 seconds watchdog timeout

void tokenStatusCallback(TokenInfo info);

void feedWatchdog() {
  esp_task_wdt_reset();
}

bool syncTimeWithRetries() {
  feedWatchdog();
  configTime(19800, 0, ntpServer); // IST timezone (UTC+5:30)
  
  for (int attempt = 1; attempt <= TIME_SYNC_RETRIES; attempt++) {
    Serial.printf("Attempting time sync (attempt %d/%d)...\n", attempt, TIME_SYNC_RETRIES);
    
    time_t now = time(nullptr);
    if (now > 1600000000) { // If we got a valid time (after year 2000)
      Serial.println("Time synchronization successful");
      return true;
    }
    
    if (attempt < TIME_SYNC_RETRIES) {
      delay(TIME_SYNC_RETRY_DELAY);
    }
  }
  
  Serial.println("Time synchronization failed after all retries");
  return false;
}

void sendResult(String time, float volt, float current, float power, int rpm, int totalRuntimeMinutes) {
  feedWatchdog();
  Serial.println("Attempting to send results to sheets...");

  for (int attempt = 1; attempt <= MAX_SHEET_RETRIES; attempt++) {
    FirebaseJson response;
    FirebaseJson valueRange;

    valueRange.add("majorDimension", "COLUMNS");
    valueRange.set("values/[0]/[0]", "Motor 1");
    valueRange.set("values/[1]/[0]", time);
    valueRange.set("values/[2]/[0]", volt);
    valueRange.set("values/[3]/[0]", current);
    valueRange.set("values/[4]/[0]", power);
    valueRange.set("values/[5]/[0]", rpm);
    valueRange.set("values/[6]/[0]", totalRuntimeMinutes);

    bool success = GSheet.values.append(&response, spreadsheetId, "Sheet1!A1", &valueRange);

    if (success) {
      Serial.println("Send result OK");
      response.toString(Serial, true);
      valueRange.clear();
      return;
    } else {
      Serial.printf("Attempt %d/%d failed: %s\n", attempt, MAX_SHEET_RETRIES, GSheet.errorReason());
      if (attempt < MAX_SHEET_RETRIES) {
        delay(5000);
      }
    }
  }
}

void tokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_error) {
    Serial.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
  }
}

String getTime() {
  feedWatchdog();
  if (!timeSyncSuccess) {
    return "Time not synced";
  }
  
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time error";
  }

  time(&now);
  now += (5 * 3600) + (30 * 60); // IST (UTC+5:30)

  struct tm istTime;
  gmtime_r(&now, &istTime);

  char timeIST[30];
  strftime(timeIST, sizeof(timeIST), "%d/%m/%Y %H:%M:%S", &istTime);
  return String(timeIST);
}

void resetRuntime() {
  resetEnergy();
  if (blynkConnected)
    Blynk.virtualWrite(V4, 0);
  if (alert)
    Blynk.logEvent("reset", "Energy has been reset to 0");
}

BLYNK_WRITE(V6) {
  if (param.asInt() == 1) {
    resetRuntime();
    delay(1000);
    if (blynkConnected)
      Blynk.virtualWrite(V6, 0);
  }
}

void IRAM_ATTR countRotation() {
  rotationCount++;
}

void calculateRPM() {
  feedWatchdog();
  unsigned long currentMillis = millis();
  if (currentMillis - lastRPMCalculation >= 1000) {
    noInterrupts();
    unsigned long count = rotationCount;
    rotationCount = 0;
    interrupts();

    RPM = (count * 60.0) / magnets;
    RPM = RPM / 10000;
    RPM = RPM / 7;

    if(PZEMPower > 5) {
      RPM = map(analogRead(34),0,4096,0,300);
      RPM = RPM/2;
    } else {
      RPM = 0.0;
    }

    lastRPMCalculation = currentMillis;

    bool currentMotorState = (RPM > 10);
    if (currentMotorState != motorRunning) {
      motorRunning = currentMotorState;
      if (motorRunning) {
        motorStartTime = currentMillis;
        motorStoppedAlertSent = false;
      } else {
        if ((currentMillis - motorStartTime) > 5000 && alert) {
          Blynk.logEvent("motor_stopped", "Motor has suddenly stopped!");
          motorStoppedAlertSent = true;
        }
      }
    }
  }
}

void checkBatteryLevel() {
  feedWatchdog();
  if (PZEMVoltage < 35.0 && !lowBatteryAlertSent && alert) {
    Blynk.logEvent("low_battery", "Battery level is below 35V!");
    lowBatteryAlertSent = true;
  } else if (PZEMVoltage >= 35.0) {
    lowBatteryAlertSent = false;
  }
}

void updateRuntime() {
  feedWatchdog();
  unsigned long currentMillis = millis();

  if (motorRunning) {
    if (!runtimeInitialized) {
      motorStartTime = currentMillis - (totalRuntimeMinutes * 60000);
      runtimeInitialized = true;
      Serial.printf("Runtime initialized at %lu minutes\n", totalRuntimeMinutes);
    }

    unsigned long newTotalRuntime = (currentMillis - motorStartTime) / 60000;

    if (newTotalRuntime != totalRuntimeMinutes) {
      totalRuntimeMinutes = newTotalRuntime;

      if (currentMillis - lastRuntimeSave >= runtimeSaveInterval) {
        EEPROM.put(eepromRuntimeAddr, totalRuntimeMinutes);
        EEPROM.commit();
        lastRuntimeSave = currentMillis;
        Serial.printf("Saved total runtime: %lu minutes\n", totalRuntimeMinutes);
      }
    }
  } else {
    runtimeInitialized = false;
  }
}

void loadRuntimeFromEEPROM() {
  feedWatchdog();
  EEPROM.get(eepromRuntimeAddr, totalRuntimeMinutes);
  Serial.printf("Loaded runtime from EEPROM: %d minutes\n", totalRuntimeMinutes);
  if (totalRuntimeMinutes > 100000) {
    totalRuntimeMinutes = 0;
    EEPROM.put(eepromRuntimeAddr, totalRuntimeMinutes);
    EEPROM.commit();
  }
}

void sendSensor() {
  feedWatchdog();
  if (blynkConnected) {
    Blynk.virtualWrite(V0, PZEMVoltage);
    Blynk.virtualWrite(V1, PZEMCurrent);
    Blynk.virtualWrite(V2, PZEMPower);
    Blynk.virtualWrite(V3, PZEMEnergy);
    Blynk.virtualWrite(V4, totalRuntimeMinutes);
    Blynk.virtualWrite(V5, RPM);
  }
}

void attemptSheetUpdate() {
  feedWatchdog();
  if (GSheet.ready() && motorRunning && !isnan(PZEMVoltage)) {
    current_time = getTime();
    if (current_time != "Time not synced" && current_time != "Time error") {
      Serial.print("Now time is :");
      Serial.println(current_time);
      sendResult(current_time, PZEMVoltage, PZEMCurrent, PZEMPower, RPM, totalRuntimeMinutes);
    } else {
      Serial.println("Time not available for sheet update");
    }
  } else {
    Serial.println("G sheets not ready");
  }
}

void checkInternetConnection() {
  feedWatchdog();
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastInternetCheck >= internetCheckInterval) {
    lastInternetCheck = currentMillis;
    
    bool hasInternet = Ping.ping("www.google.com", 3);
    
    if (hasInternet) {
      internetUnavailableSince = 0;
      Serial.println("Internet connection OK");
    } else {
      if (internetUnavailableSince == 0) {
        internetUnavailableSince = currentMillis;
        Serial.println("Internet connection lost");
      } else if (currentMillis - internetUnavailableSince >= internetUnavailableTimeout) {
        Serial.println("No internet for more than 1 minute - restarting ESP32");
        ESP.restart();
      }
    }
  }
}

void checkBlynkConnection(void *pvParameters) {
  esp_task_wdt_add(NULL);
  
  while (true) {
    feedWatchdog();
    unsigned long currentMillis = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (!blynkConnected && (currentMillis - lastBlynkConnectionAttempt >= blynkReconnectInterval)) {
        Serial.println("Attempting Blynk reconnection...");
        blynkConnected = Blynk.connect();
        lastBlynkConnectionAttempt = currentMillis;
        if (blynkConnected)
          Serial.println("Blynk reconnected!");
      }
    } else {
      blynkConnected = false;
    }
    delay(2000);
  }
}

void loop2(void *pvParameters) {
  esp_task_wdt_add(NULL);
  
  while (true) {
    feedWatchdog();
    unsigned long currentMillis = millis();
    if (currentMillis - lastSerialPrint >= serialPrintInterval) {
      uint8_t result;
      result = node.readInputRegisters(0x0000, 6);
      if (result == node.ku8MBSuccess) {
        uint32_t tempdouble = 0x00000000;
        PZEMVoltage = node.getResponseBuffer(0x0000) / 100.0;
        PZEMCurrent = node.getResponseBuffer(0x0001) / 100.0;

        tempdouble = (node.getResponseBuffer(0x0003) << 16) + node.getResponseBuffer(0x0002);
        PZEMPower = tempdouble / 10.0;

        tempdouble = (node.getResponseBuffer(0x0005) << 16) + node.getResponseBuffer(0x0004);
        PZEMEnergy = tempdouble;

        Previous_Voltage = PZEMVoltage;
        Previous_Current = PZEMCurrent;
        Previous_Power = PZEMPower;
        Previous_Energy = PZEMEnergy;

        checkBatteryLevel();
        calculateRPM();
        updateRuntime();
      } else {
        PZEMVoltage = Previous_Voltage;
        PZEMCurrent = Previous_Current;
        PZEMPower = Previous_Power;
        PZEMEnergy = Previous_Energy;
      }

      lastSerialPrint = currentMillis;

      Serial.print("Vdc: ");
      Serial.print(PZEMVoltage);
      Serial.println(" V");
      Serial.print("Idc: ");
      Serial.print(PZEMCurrent);
      Serial.println(" A");
      Serial.print("Power: ");
      Serial.print(PZEMPower);
      Serial.println(" W");
      Serial.print("Energy: ");
      Serial.print(PZEMEnergy);
      Serial.println(" Wh");
      Serial.print("RPM: ");
      Serial.print(RPM);
      Serial.println(" rpm");
      Serial.print("Run Time: ");
      Serial.print(totalRuntimeMinutes);
      Serial.println(" min");
      Serial.print("Time: ");
      Serial.println(getTime());
      Serial.println("----------------------");
    }
    delay(5000);
  }
}

void googleSheetsTask(void *pvParameters) {
  esp_task_wdt_add(NULL);
  
  unsigned long lastTaskRun = millis();
  const unsigned long initialDelay = 30000;

  while (true) {
    feedWatchdog();
    unsigned long currentMillis = millis();

    static bool firstRun = true;
    if (firstRun && (currentMillis - lastTaskRun >= initialDelay)) {
      firstRun = false;
      lastTaskRun = currentMillis;
      Serial.println("Initial Google Sheets update");
      attemptSheetUpdate();
    } else if (!firstRun && (currentMillis - lastTaskRun >= sheetUpdateInterval)) {
      Serial.println("Google Sheets updating... ");
      lastTaskRun = currentMillis;
      attemptSheetUpdate();
    }

    delay(1000);
  }
}

void setup() {
  // Initialize hardware watchdog
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  Serial.begin(115200);
  delay(1000);

  EEPROM.begin(512);
  loadRuntimeFromEEPROM();

  // Initialize WiFi with timeout
  WiFi.begin(ssid, pass);
  Serial.println("Connecting to WiFi...");
  
  unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < 30000) {
    delay(500);
    Serial.print(".");
    feedWatchdog();
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed - restarting");
    ESP.restart();
  }
  Serial.println("\nWiFi connected");

  // Attempt time synchronization with retries
  timeSyncSuccess = syncTimeWithRetries();
  if (!timeSyncSuccess) {
    Serial.println("Critical time sync failure - restarting ESP32");
    ESP.restart();
  }

  GSheet.printf("ESP Google Sheet Client v%s\n\n", ESP_GOOGLE_SHEET_CLIENT_VERSION);

  pinMode(hallSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hallSensorPin), countRotation, FALLING);

  Blynk.config(auth);
  blynkConnected = Blynk.connect();

  GSheet.setTokenCallback(tokenStatusCallback);
  GSheet.setPrerefreshSeconds(10 * 60);
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

  pzem_begin();

  timer.setInterval(360000L, sendSensor);

  xTaskCreate(checkBlynkConnection, "checkBlynkConnection", 2000, NULL, 1, NULL);
  delay(1000);
  xTaskCreate(loop2, "loop2", 8000, NULL, 1, NULL);
  delay(5000);
  xTaskCreate(googleSheetsTask, "GoogleSheetsTask", 12288, NULL, 1, NULL);
  delay(1000);

  Serial.println("Setup complete");
}

void loop() {
  feedWatchdog();
  
  Blynk.run();
  timer.run();
  checkInternetConnection();
  
  delay(100);
}
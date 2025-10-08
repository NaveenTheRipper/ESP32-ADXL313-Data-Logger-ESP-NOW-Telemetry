/*
 * ESP32 ADXL313 Data Logger with ESP-NOW Telemetry
 * -------------------------------------------------
 * Features:
 *  - Collects 3-axis accelerometer data (ADXL313)
 *  - Logs timestamped data to SD card (CSV format)
 *  - Transmits telemetry via ESP-NOW
 *  - FreeRTOS tasks for parallel execution
 *  - NTP-based time synchronization
 *  - Automatic suspend/resume and restart scheduling
 *
 * Author: - Naveen Ranasinghe <naveenranasingha@gmail.com>
 * Created: August 2025
 *
 * Copyright (c) 2025 - Naveen Ranasinghe
 * 
 * Licensed under the MIT License. See LICENSE file or:
 *     https://opensource.org/licenses/MIT
 */

#include <SparkFunADXL313.h>
#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "sdios.h"
#include <SPI.h>
#include "SdFat.h"
#include "time.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#define error(msg) sd.errorHalt(F(msg))

// ---------- Global Objects ----------
SdFat sd;
SdFile file;
ADXL313 myAdxl;

const int chipSelect = 5;
int mint, sect, hourt, dayt, montht, yeart;

const char* ssid = "****";
const char* password = "******";

char gpsDate[20];
char header[20];
char Date[20];
char cap[20];

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int daylightOffset_sec = 3600;

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// ESP-NOW peer (receiver MAC)
uint8_t broadcastAddress[] = {0x7C, 0x9E, 0xBD, 0x30, 0x6E, 0x98};

// Shared data buffer
float data[3];

// Data packet structure
typedef struct struct_message {
  int id;
  float x;
  float y;
  float z;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// ---------- Callbacks ----------
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  // Uncomment for debug:
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// ---------- Utility ----------
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  hourt  = timeinfo.tm_hour;
  mint   = timeinfo.tm_min;
  sect   = timeinfo.tm_sec;
  dayt   = timeinfo.tm_mday;
  montht = timeinfo.tm_mon + 1;
  yeart  = timeinfo.tm_year + 1900;
}

// ---------- FreeRTOS Tasks ----------
void Task1code(void* pvParameters) {
  for (;;) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = 1;

    if (myAdxl.dataReady()) {
      myAdxl.readAccel();

      data[0] = myAdxl.x;
      data[1] = myAdxl.y;
      data[2] = myAdxl.z;

      sprintf(Date, "%d/%d/%d %d:%d:%d", yeart, montht, dayt, hourt, mint, sect);
      sprintf(gpsDate, "%s,%d,%d,%d,\n", Date, int(myAdxl.x), int(myAdxl.y), int(myAdxl.z));

      ofstream sdout(cap, ios::out | ios::app);
      if (!sdout) error("open failed");

      sdout << gpsDate;
      sdout.close();
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void Task2code(void* pvParameters) {
  for (;;) {
    TickType_t xLastWakeTime1 = xTaskGetTickCount();
    const TickType_t xFrequency1 = 1;

    printLocalTime();

    // Scheduled suspend
    if (hourt == 21 && mint == 12 && sect == 10) {
      vTaskSuspend(Task1);
      vTaskSuspend(Task3);
      myAdxl.standby();
      Serial.println("Tasks suspended");
    }

    // Scheduled resume
    if (hourt == 6 && mint == 12 && sect == 10) {
      vTaskResume(Task1);
      vTaskResume(Task3);
      myAdxl.measureModeOn();
      Serial.println("Tasks resumed");
    }

    // Scheduled restart
    if ((hourt == 6 && mint == 16 && sect == 10) ||
        (hourt == 6 && mint == 18 && sect == 10)) {
      ESP.restart();
    }

    vTaskDelayUntil(&xLastWakeTime1, xFrequency1);
  }
}

void Task3code(void* pvParameters) {
  for (;;) {
    WiFi.mode(WIFI_STA);

    myData.id = 11;
    myData.x = data[0];
    myData.y = data[1];
    myData.z = data[2];

    esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    WiFi.mode(WIFI_OFF);
  }
}

// ---------- Setup ----------
void setup() {
  Wire.begin();
  Serial.begin(115200);

  // Connect Wi-Fi for time sync
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Ensure valid time
retry_time:
  printLocalTime();
  if (yeart == 0) goto retry_time;

  sprintf(cap, "/test%d%d%d.csv", yeart, montht, dayt);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Init ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Init ADXL313
  if (!myAdxl.begin()) while (1);
  myAdxl.standby();
  myAdxl.setRange(ADXL313_RANGE_2_G);
  myAdxl.setBandwidth(ADXL313_BW_50);
  myAdxl.setFullResBit(true);
  myAdxl.measureModeOn();

  // Init SD
  if (!sd.begin(chipSelect, SD_SCK_MHZ(10))) sd.initErrorHalt();
  if (!sd.exists(cap)) {
    if (!file.open(cap, O_WRONLY | O_CREAT | O_EXCL)) error("!file.open");
    sprintf(header, "%s,%s,%s,%s,", "Date", "X", "Y", "Z");
    file.print(F(header));
    file.println();
    file.close();
  }

  // Test send
  myData.id = 11; myData.x = 5; myData.y = 5; myData.z = 5;
  esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));
  WiFi.mode(WIFI_OFF);

  // Launch tasks
  xTaskCreatePinnedToCore(Task1code, "Task1", 20000, NULL, 5, &Task1, 0);
  xTaskCreatePinnedToCore(Task2code, "Task2", 10000, NULL, 6, &Task2, 1);
  xTaskCreatePinnedToCore(Task3code, "Task3", 20000, NULL, 1, &Task3, 1);
}

void loop() {
  // Nothing — all work handled by FreeRTOS tasks
}

/*
  Task writeToEspNow:
    Dequeue data from ESP-NOW queue, forward to ESP-NOW:
      send_to_EspNow_Queue --> writeToEspNow() --> Outgoing ESP-NOW packet
  
  Task espnowHeartbeat:
     Send uptime and stack data to send_to_EspNow_Queue
     Send uptime and stack data to Nextion serial port
*/
#include "espnow.h"
#include <Arduino.h>

// MAC Address of ESP_NOW receiver
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

// TaskHandle_t xhandleSerialWriteHandle = NULL;
TaskHandle_t xhandleEspNowWriteHandle = NULL;
TaskHandle_t xhandleHeartbeat = NULL;

// static QueueHandle_t send_to_Serial_queue;
static QueueHandle_t send_to_EspNow_queue;

char uptimeBuffer[12];  // scratch space for storing formatted 'uptime' string

// Calculate uptime & populate uptime buffer for future use
void uptime() {
  // Constants for uptime calculations
  static const uint32_t millis_in_day = 1000 * 60 * 60 * 24;
  static const uint32_t millis_in_hour = 1000 * 60 * 60;
  static const uint32_t millis_in_minute = 1000 * 60;

  uint8_t days = millis() / (millis_in_day);
  uint8_t hours = (millis() - (days * millis_in_day)) / millis_in_hour;
  uint8_t minutes = (millis() - (days * millis_in_day) - (hours * millis_in_hour)) / millis_in_minute;
  snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%2dd%2dh%2dm", days, hours, minutes);
}

// Tasks

// Read from ESP Send queue and forward to ESP-NOW
// ToDo check string length and send only length bytes
// Check and print ESP error message
void writeToEspNow(void *parameter) {
  char sendBuffer[ESP_BUFFER_SIZE];
  for (;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    // Dequeue
    if (xQueueReceive(send_to_EspNow_queue, sendBuffer, portMAX_DELAY) == pdTRUE) {
      Serial.println("writeToEspNow" + (String)sendBuffer);
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)sendBuffer, ESP_BUFFER_SIZE);
    }
  }
}

// Send heartbeat out to ESP_NOW broadcastAddress
void espnowHeartbeat(void *parameter) {
  String jsonMessage = "";
  jsonMessage.reserve(ESP_BUFFER_SIZE + 2);
  StaticJsonDocument<ESP_BUFFER_SIZE> doc;
  for (;;) {
    uint64_t now = esp_timer_get_time() / 1000 / 1000;
    uptime();
    doc.clear();
    jsonMessage.clear();
    doc["D"] = DEVICE_NAME;
    doc["T"] = uptimeBuffer;
    doc["R"] = uxTaskGetStackHighWaterMark(xhandleHeartbeat);
    doc["W"] = uxTaskGetStackHighWaterMark(xhandleEspNowWriteHandle);
    doc["H"] = esp_get_minimum_free_heap_size();

    serializeJson(doc, jsonMessage);  // Convert JsonDoc to JSON string
    if (!espNowSend(jsonMessage)) {
      Serial.print("Error sending data: ");
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

// Functions
// Send String to ESP-NOW peers (broadcast address)
bool espNowSend(const String &message) {
  if (message.length() <= ESP_BUFFER_SIZE) {
    if (xQueueSend(send_to_EspNow_queue, message.c_str(), 0) == pdTRUE) {
      return true;
    } else {
      Serial.println("Error sending to queue");
      return false;
    }
  }
  return false;
}

// Send JSON doc to ESP-NOW peers (broadcast address)
bool espNowSend(const JsonDocument &doc) {
  String jsonMessage = "";
  serializeJson(doc, jsonMessage);  // Convert JsonDoc to JSON string
  if (jsonMessage.length() <= ESP_BUFFER_SIZE) {
    if (xQueueSend(send_to_EspNow_queue, jsonMessage.c_str(), 0) == pdTRUE) {
      return true;
    } else {
      Serial.println("Error sending to queue");
      return false;
    }
  }
  return false;
}

// Initialize ESP_NOW interface. Call once from setup()
bool initEspNow() {
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  } else {
    Serial.println("ESP-NOW initialized");
  }

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return false;
  }
  // Set up queues and tasks

  send_to_EspNow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, ESP_BUFFER_SIZE);
  if (send_to_EspNow_queue == NULL) {
    Serial.println("Create Queue failed");
    return false;
  }
  xTaskCreate(espnowHeartbeat, "Heartbeat Handler", 2400, NULL, 4, &xhandleHeartbeat);
  // xTaskCreate(writeToNextion, "Nextion Serial Write Handler", 2400, NULL, 4, &xhandleSerialWriteHandle);
  xTaskCreate(writeToEspNow, "ESP_NOW Write Handler", 2000, NULL, 4, &xhandleEspNowWriteHandle);

  return true;
}

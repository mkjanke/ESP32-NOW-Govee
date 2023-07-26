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
      Serial.print("writeToEspNow");
      Serial.println(sendBuffer);
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)sendBuffer, ESP_BUFFER_SIZE);
    }
  }
}

// Send heartbeat out to ESP_NOW broadcastAddress
void espnowHeartbeat(void *parameter) {
  std::string jsonString = "";
  jsonString.reserve(ESP_BUFFER_SIZE + 2);
  for (;;) {
    uptime();
    {
      StaticJsonDocument<ESP_BUFFER_SIZE> doc;
      doc["D"] = DEVICE_NAME;
      doc["T"] = uptimeBuffer;
      doc["R"] = uxTaskGetStackHighWaterMark(xhandleHeartbeat);
      doc["W"] = uxTaskGetStackHighWaterMark(xhandleEspNowWriteHandle);
      doc["Q"] = uxQueueMessagesWaiting(send_to_EspNow_queue);
      doc["H"] = esp_get_minimum_free_heap_size();

      serializeJson(doc, jsonString);  // Convert JsonDoc to JSON string
      if (!espNowSend(jsonString)) {
        Serial.print("Error sending data: ");
      }
    }
    jsonString.clear();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

// Functions

// Queue message to send_to_EspNow_queue
bool espNowSend(const char *charMessage) {
  char buffer[ESP_BUFFER_SIZE] = {0};
  if (strlen(charMessage) <= ESP_BUFFER_SIZE - 1) {
    strncpy(buffer, charMessage, ESP_BUFFER_SIZE);  // copy incoming char[] into cleared buffer
    if (xQueueSend(send_to_EspNow_queue, buffer, 0) == pdTRUE) {
      return true;
    } else {
      Serial.println("Error sending to queue");
      return false;
    }
  }
  return false;
}

bool espNowSend(const std::string &stringMessage) {
  char charMessage[ESP_BUFFER_SIZE] = {0};
  if (stringMessage.size() <= ESP_BUFFER_SIZE) {
    std::copy(stringMessage.begin(), stringMessage.end(), charMessage);
    if (xQueueSend(send_to_EspNow_queue, charMessage, 0) == pdTRUE) {
      return true;
    } else {
      Serial.println("Error sending to queue");
      return false;
    }
  }
  return false;
}

bool espNowSend(const JsonDocument &doc) {
  char jsonMessage[ESP_BUFFER_SIZE] = {0};
  if (serializeJson(doc, jsonMessage, ESP_BUFFER_SIZE) <= ESP_BUFFER_SIZE) {
    if (xQueueSend(send_to_EspNow_queue, jsonMessage, 0) == pdTRUE) {
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
  xTaskCreate(writeToEspNow, "ESP_NOW Write Handler", 2000, NULL, 4, &xhandleEspNowWriteHandle);

  return true;
}

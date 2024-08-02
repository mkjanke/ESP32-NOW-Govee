/*
  Task task_writeToEspNow:
    Dequeue data from ESP-NOW queue, forward to ESP-NOW:
      send_to_EspNow_Queue --> task_writeToEspNow() --> Outgoing ESP-NOW packet

  Task espnowHeartbeat:
     Send uptime and stack data to send_to_EspNow_Queue
     Send uptime and stack data to Nextion serial port
*/
#include "espnow.h"

#include <Arduino.h>

// MAC Address of ESP_NOW receiver (broadcast address)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

/// @brief handle to task that reads msgs from send_to_EspNow_queue and
///        writes msgs to ESP-NOW broadcast address
TaskHandle_t xhandleEspNowWriteHandle = NULL;

/// @brief Periodic heartbeat packet sent to ESP-NOW broadcast address
TaskHandle_t xhandleHeartbeat = NULL;

/// @brief Queue to handle messages received from BLE
///        and sent to ESP-NOW
static QueueHandle_t send_to_EspNow_queue;

/// @brief Calculate uptime & populate uptime buffer for future use
/// @param buffer char * buffer to store uptime information
/// @param size Size of uptime buffer
void uptime(char *buffer, uint8_t size) {
  // Constants for uptime calculations
  static const uint32_t millis_in_day = 1000 * 60 * 60 * 24;
  static const uint32_t millis_in_hour = 1000 * 60 * 60;
  static const uint32_t millis_in_minute = 1000 * 60;

  unsigned long now = millis();
  uint8_t days = now / (millis_in_day);
  uint8_t hours = (now - (days * millis_in_day)) / millis_in_hour;
  uint8_t minutes = (now - (days * millis_in_day) - (hours * millis_in_hour)) / millis_in_minute;
  snprintf(buffer, size, "%2dd%2dh%2dm", days, hours, minutes);
}

/// @brief  Read from queue send_to_EspNow_queue and forward to ESP-NOW
///         Check and print ESP error message
/// @param parameter
void task_writeToEspNow(void *parameter) {
  char sendBuffer[ESP_BUFFER_SIZE];
  for (;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    // Dequeue
    if (xQueueReceive(send_to_EspNow_queue, sendBuffer, portMAX_DELAY) == pdTRUE) {
      Serial.print("task_writeToEspNow");
      Serial.println(sendBuffer);
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)sendBuffer, ESP_BUFFER_SIZE);
    }
  }
}

/// @brief Send periodic heartbeat out to ESP_NOW broadcast address
/// @param parameter
void espnowHeartbeat(void *parameter) {
  char uptimeBuffer[12];  // scratch space for storing formatted 'uptime' string

  for (;;) {
    uptime(uptimeBuffer, sizeof(uptimeBuffer));
    {
      char buffer[ESP_BUFFER_SIZE] = {0};
      JsonDocument doc;
      doc["D"] = DEVICE_NAME;
      doc["T"] = uptimeBuffer;
      doc["R"] = uxTaskGetStackHighWaterMark(xhandleHeartbeat);
      doc["W"] = uxTaskGetStackHighWaterMark(xhandleEspNowWriteHandle);
      doc["H"] = esp_get_free_heap_size();
      doc["M"] = esp_get_minimum_free_heap_size();

      serializeJson(doc, buffer);  // Convert JsonDoc to JSON string
      if (!espNowSend(buffer)) {
        Serial.print("Error sending data: ");
      }
    }
    vTaskDelay(HEARTBEAT / portTICK_PERIOD_MS);
  }
}

/// @brief Queue char * message to send_to_EspNow_queue
/// @param charMessage char * message to be sent to ESP-NOW
/// @return Success or failure
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

/// @brief Queue std::string message to send_to_EspNow_queue
/// @param stringMessage std::string message to be sent to ESP-NOW
/// @return Success or failure
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

/// @brief Queue JsonDocument message to send_to_EspNow_queue
/// @param doc JsonDocument document to be sent to ESP-NOW
/// @return Success or failure
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

/// @brief Initialize ESP-NOW interface, register callbacks
///        Call once from setup()
/// @return True upon success, else false
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
  xTaskCreate(task_writeToEspNow, "ESP_NOW Write Handler", 2000, NULL, 4, &xhandleEspNowWriteHandle);

  return true;
}

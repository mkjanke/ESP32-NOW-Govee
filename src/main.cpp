/*
GOVEE 5074 Temperature scanner for ESP32

Proof of concept. May or may not leak memory, crash, etc.

Not valid for temperature values below 0C.

Uses ArduinoJSON, NimBLE, ESP-NOW

Listens for BLE advertisements from any GOVEE 5074 sensor
  - extracts temperature and humidity from BLE advertisement
  - creates JSON doc
  - forwards doc to ESP-NOW broadcast address

Sample ESP-NOW packet:
{
  "D":"ESP-GOVEE",
  "Govee_H5074_DB3A/tempInC":18.87999916,
  "Govee_H5074_DB3A/humidity":51.66999817,
  "Govee_H5074_DB3A/battery":97
}

*/
#include <Arduino.h>
#include "settings.h"
#include "NimBLEDevice.h"
#include "espnow.h"

NimBLEScan* pBLEScan;
NimBLEUUID serviceUuid("ec88");  // Govee 5074 serivice UUID

extern uint8_t broadcastAddress[];  // ESP-NOW broadcast Address

// Create JSON doc and forward to ESP-NOW queue
bool createAndSendJSON(const std::string& deviceName, double tempInC, double humidity, float batteryPct) {
  char buffer[ESP_BUFFER_SIZE] = {0};
  StaticJsonDocument<ESP_BUFFER_SIZE * 2> doc;

  doc["D"] = DEVICE_NAME;
  doc[deviceName + "/tempInC"] = tempInC;
  doc[deviceName + "/humidity"] = humidity;
  doc[deviceName + "/battery"] = batteryPct;

  if (serializeJson(doc, buffer) <= ESP_BUFFER_SIZE) {
    bool result = espNowSend(buffer);
    doc.clear();
    return result;
  }
  doc.clear();
  return false;
}

class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  // Called when BLE scan sees BLE advertisement.
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    // check for Govee 5074 service UUID
    if (advertisedDevice->getServiceUUID() == serviceUuid) {
      // Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());

      // Desired Govee advert will have 9 byte mfg. data length & leading bytes 0x88 0xec
      if ((advertisedDevice->getManufacturerData().length() == 9) &&
          ((byte)advertisedDevice->getManufacturerData().data()[0] == 0x88) &&
          ((byte)advertisedDevice->getManufacturerData().data()[1] == 0xec)) {
        // Extract temperature, humidity and battery pct.
        double tempInC = ((double)((int16_t)((advertisedDevice->getManufacturerData()[3] << 0) |
                                             (advertisedDevice->getManufacturerData()[4]) << 8))) /
                         100;
        double humPct = ((double)((int16_t)((advertisedDevice->getManufacturerData()[5] << 0) |
                                            (advertisedDevice->getManufacturerData()[6]) << 8))) /
                        100;
        uint8_t battPct = (uint8_t)advertisedDevice->getManufacturerData()[7];

        // char* payloaddata = NimBLEUtils::buildHexData(NULL, (uint8_t*)advertisedDevice->getPayload(),
        // advertisedDevice->getPayloadLength()); Serial.println(payloaddata); if (payloaddata != NULL) {
        //   free(payloaddata);
        // }

        Serial.print(advertisedDevice->getName().c_str());
        Serial.printf(" %4.2f %4.2f %d\n", tempInC, humPct, battPct);

        // Send it off to ESP_NOW
        createAndSendJSON(advertisedDevice->getName(), tempInC, humPct, battPct);
      }
    } else {
      // Serial.println("Erasing Device " + (String)advertisedDevice->toString().c_str());
      pBLEScan->erase(advertisedDevice->getAddress());
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.printf("%s Is Now Woke!\n", DEVICE_NAME);

  NimBLEDevice::setScanDuplicateCacheSize(10);
  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();  // create new scan
  // Set the callback for when devices are discovered, include duplicates.
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(true);  // Set active scanning, this will get more data from the advertiser.
  // Values taken from NimBLE examples
  pBLEScan->setInterval(97);   // How often the scan occurs / switches channels; in milliseconds,
  pBLEScan->setWindow(37);     // How long to scan during the interval; in milliseconds.
  pBLEScan->setMaxResults(0);  // do not store the scan results, use callback only.

  // Start esp-now
  WiFi.mode(WIFI_STA);
  if (!initEspNow()) {
    return;
  };
}

void loop() {
  // If an error occurs that stops the scan, it will be restarted here.
  if (pBLEScan->isScanning() == false) {
    // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
    Serial.println("Restarting BLE scan");
    pBLEScan->start(0, nullptr, false);
  }
  // Free memory from unused devices?
  if (pBLEScan->getResults().getCount() > 10) {
    pBLEScan->clearResults();
  }
  delay(2000);
}

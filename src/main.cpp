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
  "address":"a4:c1:38:cb:db:3a",
  "deviceName":"Govee_H5074_DB3A",
  "tempInC":18.87999916,
  "humidity":51.66999817,
  "battery":97
}

*/

#include <Arduino.h>
#include "espnow.h"
#include "NimBLEDevice.h"

#include "settings.h"

NimBLEScan* pBLEScan;

NimBLEUUID serviceUuid("ec88");   //Govee 5074 serivice UUID

extern uint8_t broadcastAddress[]; //ESP-NOW broadcast Address

// Create JSON doc and forward to ESP-NOW queue
bool createAndSendJSON(const std::string & address, const std::string & deviceName, float tempInC, float humidity, float batteryPct){
  StaticJsonDocument<ESP_BUFFER_SIZE> doc;
  doc.clear();
  doc["D"] = DEVICE_NAME;
  doc["address"] = address;
  doc["deviceName"] = deviceName;
  doc["tempInC"] = tempInC;
  doc["humidity"] = humidity;
  doc["battery"] = batteryPct;
  return espNowSend((JsonDocument&)doc);
}

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

    //Called when BLE scan sees BLE advertisement.
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
      //check for Govee 5074 service UUID
      if (advertisedDevice->getServiceUUID() == serviceUuid){
        // Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
        
        // Desired Govee advert will have 9 byte mfg. data length & leading bytes 0x88 0xec 
        if ((advertisedDevice->getManufacturerData().length() == 9) &&
            ((byte)advertisedDevice->getManufacturerData().data()[0] == 0x88) &&
            ((byte)advertisedDevice->getManufacturerData().data()[1] == 0xec)) {
            
            std::string output = advertisedDevice->getName() + " " + advertisedDevice->getAddress().toString() + " ";
            Serial.print(output.c_str());

            // Extract temperature, humidity and battery pct.
            uint8_t* MFRdata;
            MFRdata = (uint8_t*)advertisedDevice->getManufacturerData().data();
            float tempInC = ((float)((uint16_t)((MFRdata[3] << 0) | (MFRdata[4]) << 8)))/100;
            float humPct  = ((float)((uint16_t)((MFRdata[5] << 0) | (MFRdata[6]) << 8)))/100;
            uint8_t battPct = (uint8_t)MFRdata[7]; 

            Serial.printf(" %4.2f %4.2f %d\n", tempInC, humPct, battPct);
            
            // Send it off tto ESP_NOW
            createAndSendJSON(advertisedDevice->getAddress().toString(),
                    advertisedDevice->getName(),
                    tempInC,
                    humPct,
                    battPct
                    );
        }

      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.printf("%s Is Now Woke!\n", DEVICE_NAME);

  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan(); //create new scan
  // Set the callback for when devices are discovered, include duplicates.
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
  // Values taken from NimBLE examples
  pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
  pBLEScan->setWindow(37);  // How long to scan during the interval; in milliseconds.
  pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.

  // Start esp-now
  WiFi.mode(WIFI_STA); 
  if (!initEspNow()) {
    return;
  };

}

void loop() {
  // If an error occurs that stops the scan, it will be restarted here.
  if(pBLEScan->isScanning() == false) {
      // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
      pBLEScan->start(0, nullptr, false);
  }

  delay(2000);
}


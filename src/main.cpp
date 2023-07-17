#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>
#include "NimBLEDevice.h"

#include "settings.h"

NimBLEScan* pBLEScan;

//Govee 5074 serivice UUID
NimBLEUUID serviceUuid("ec88");

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
      if (advertisedDevice->getServiceUUID() == serviceUuid){
        // Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
        
        // Desired advert will have 9 byte mfg. data length & leading bytes 0x88 0xec 
        if ((advertisedDevice->getManufacturerData().length() == 9) &&
            ((byte)advertisedDevice->getManufacturerData().data()[0] == 0x88) &&
            ((byte)advertisedDevice->getManufacturerData().data()[1] == 0xec)) {
            
            // char* payloaddata = NimBLEUtils::buildHexData(NULL, (uint8_t*)advertisedDevice->getPayload(), advertisedDevice->getPayloadLength());
            // Serial.println(payloaddata);
            // if (payloaddata != NULL) {
            //   free(payloaddata);
            // }

          std::string output = advertisedDevice->getName() + " " + advertisedDevice->getAddress().toString() + " ";
          Serial.print(output.c_str());

          char* manufacturerdata = NimBLEUtils::buildHexData(NULL, (uint8_t*)advertisedDevice->getManufacturerData().data(), advertisedDevice->getManufacturerData().length());
          Serial.print(manufacturerdata);
          if (manufacturerdata != NULL) {
            free(manufacturerdata);
          }

          uint8_t* MFRdata;
          MFRdata = (uint8_t*)advertisedDevice->getManufacturerData().data();
          float tempInC = ((float)((uint16_t)((MFRdata[3] << 0) | (MFRdata[4]) << 8)))/100;
          float humPct  = ((float)((uint16_t)((MFRdata[5] << 0) | (MFRdata[6]) << 8)))/100;
          uint8_t battPct = (uint8_t)MFRdata[7]; 

          Serial.printf(" %4.2f %4.2f %d\n", tempInC, humPct, battPct);

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

}

void loop() {
  // If an error occurs that stops the scan, it will be restarted here.
  if(pBLEScan->isScanning() == false) {
      // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
      pBLEScan->start(0, nullptr, false);
  }

  delay(2000);
}


# GOVEE 5074 Temperature scanner for ESP32 

Proof of concept. May or may not leak memory, crash, etc.

Not valid for temperature values below 0C.

Uses ArduinoJSON, NimBLE, ESP-NOW

Listens for BLE advertisements from any GOVEE 5074 sensor 
  - extracts temperature and humidity from BLE advertisement
  - creates JSON doc
  - forwards doc to ESP-NOW broadcast address


Sample ESP-NOW packet:
```
{
  "D":"ESP-GOVEE",
  "address":"a4:c1:38:cb:db:3a",
  "deviceName":"Govee_H5074_DB3A",
  "tempInC":18.87999916,
  "humidity":51.66999817,
  "battery":97
}
```

# Govee 5074 decode notes

Notes on passively monitoring Govee BLE 5074 temperature sensor using and ESP32 

Reference: https://github.com/Thrilleratplay/GoveeWatcher

## Govee 5074 Advertising Packet (NimBLE getPayload()):

```
Byte: 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35
      -----------------------------------------------------------------------------------------------------------
Data: 02 01 06 03 03 88 ec 11 09 47 6f 76 65 65 5f 48 35 30 37 34 5f 46 37 31 41 0a ff 88 ec 00 58 0a e6 11 64 02
Data: 02 01 06 03 03 88 ec 11 09 47 6f 76 65 65 5f 48 35 30 37 34 5f 46 37 31 41 0a ff 88 ec 00 b7 09 a0 12 64 02
                     -----       -----------------------------------------------       -----    ----- ----- --
                     Mfg Id       G  o  v  e  e  _  H  5  0  7  4  _  F  7  1  A       Mfg Id   TempC Humid Bat
```

##Mfg. Data Field (NimBLE getManufacturerData().data())

```
88 ec 00 f8 09 cf 11 64 02
-----    ----- ----- -- 
Mfg Id   TempC Humid Bat
```

### Example Mfg. data:
```
88ec00580ae6116402
```
Temperature (C): `580a` 
Convert to int: `data[3] << 0) | (data[4]) << 8 = 2648` 
Divide by 100 = 26.48 deg. C

Humidity: `e611`
Convert to int: `data[5] << 0) | (data[6]) << 8 = 4582` 
Divide by 100 = 45.82 %

Battery Pct: 64 = 64 hex = 100%

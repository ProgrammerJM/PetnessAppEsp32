# Petness App

This is a project for managing pet feeding schedules and weight tracking using an ESP32, HX711 ADC, and Firebase Realtime Database.

## Dependencies

This project uses the following libraries:

- FirebaseESP32
- ArduinoJson
- HX711_ADC
- WiFi

## Setup

1. Install the PlatformIO and add support for the ESP32 board.
2. Install the required libraries (FirebaseESP32, ArduinoJson, HX711_ADC, WiFi) using the Library Manager in the Arduino IDE.
3. Replace the `WIFI_SSID` and `WIFI_PASSWORD` constants with your WiFi credentials.
4. Replace the `FIREBASE_HOST` and `FIREBASE_AUTH` constants with your Firebase Realtime Database URL and secret.

## Hardware

- ESP32
- HX711 ADC

## Functionality

The ESP32 connects to the Firebase Realtime Database and listens for changes in the `/trigger/getPetWeight/status` and `/petFeedingSchedule` paths. 

When the status in `/trigger/getPetWeight/status` changes, the ESP32 reads the weight from the HX711 ADC and sends it back to the Firebase Realtime Database.

The ESP32 also reads the feeding schedule from the `/petFeedingSchedule` path and dispenses the appropriate amount of food according to the schedule.

# Arduino PM Box Sensor System

## Overview
Arduino-based PM Box sensor system designed to monitor air quality and environmental parameters. The system collects data from various sensors and transmits it to Firebase for real-time monitoring and historical analysis.

## Hardware Components
- Arduino (ESP8266 or ESP32)
- PM2.5 and PM10 sensor (PMS5003, PMS7003, or equivalent)
- Temperature and humidity sensor (DHT22 or DHT11)
- MQ-3 alcohol sensor
- Power supply (5V)
- Jumper wires
- Micro USB cable
- Enclosure box

## Wiring Diagram
### ESP8266/ESP32 Connections

| Sensor | Arduino Pin | Description |
|--------|-------------|-------------|
| PMS5003 TX | D3 (GPIO 0) | PM Sensor data transmission |
| PMS5003 VCC | 5V | Power supply |
| PMS5003 GND | GND | Ground |
| DHT22 DATA | D4 (GPIO 2) | Temperature/humidity data |
| DHT22 VCC | 3.3V | Power supply |
| DHT22 GND | GND | Ground |
| MQ-3 AO | A0 | Alcohol sensor analog output |
| MQ-3 VCC | 3.3V | Power supply |
| MQ-3 GND | GND | Ground |

## Software Requirements

- Arduino IDE (1.8.x or later)
- Required Libraries:
  - ESP8266WiFi or WiFiESP32 (based on your board)
  - FirebaseESP8266 or FirebaseESP32
  - SoftwareSerial
  - DHT sensor library
  - ArduinoJson (version 6.x)
  - NTPClient (for timestamp)

## Installation

1. Install Arduino IDE
2. Add ESP8266/ESP32 board manager URL in Preferences:
   - ESP8266: http://arduino.esp8266.com/stable/package_esp8266com_index.json
   - ESP32: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
3. Install the board package from Board Manager
4. Install all required libraries from Library Manager
5. Connect your Arduino board to your computer
6. Open the PM Box sketch
7. Configure WiFi credentials and Firebase details
8. Upload the sketch to your board

## Configuration
Edit the following parameters in the sketch before uploading:

```cpp
// WiFi settings
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Firebase settings
#define FIREBASE_HOST "pm-box-data-visual-default-rtdb.asia-southeast1.firebaseapp.com" 
#define FIREBASE_AUTH "YOUR_FIREBASE_AUTH_KEY"

// Time settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 7 * 3600  // Thailand time zone (GMT+7)
```

## Main Sketch Code

```cpp
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <HardwareSerial.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// Configuration definitions
// ... (as above)

// Sensor pins
#define DHTPIN 4  // GPIO 4
#define DHTTYPE DHT22
#define MQ3_PIN 34 // Analog GPIO 34

// Hardware serial for PMS sensor
HardwareSerial pmsSerial(1); // UART1

// Initialize sensors
DHT dht(DHTPIN, DHTTYPE);

// Firebase objects
FirebaseData firebaseData;
FirebaseJson json;

// NTP settings for time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, GMT_OFFSET_SEC);

// Global variables
float temperature = 0;
float humidity = 0;
float alcoholValue = 0;
int pm10 = 0;
int pm25 = 0;
unsigned long lastSensorRead = 0;
unsigned long lastFirebaseUpdate = 0;

void setup() {
  Serial.begin(9600);
  pmsSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=GPIO16, TX=GPIO17
  dht.begin();
  
  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Initialize Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  
  // Configure Firebase timeout
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

  // Start NTP client
  timeClient.begin();
}

void loop() {
  // Update time
  timeClient.update();
  
  // Read sensors every 2 seconds
  if (millis() - lastSensorRead > 2000) {
    readSensors();
    lastSensorRead = millis();
  }
  
  // Update Firebase every 1 minute
  if (millis() - lastFirebaseUpdate > 60000) {
    updateFirebase();
    lastFirebaseUpdate = millis();
  }
}

void readSensors() {
  // Read temperature and humidity
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  
  // Check if DHT readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  // Read alcohol sensor
  alcoholValue = analogRead(MQ3_PIN);
  alcoholValue = (alcoholValue / 4095) * 10; // Convert to 0-10 scale (ESP32 has 12-bit ADC)
  
  // Read PM sensor
  if (readPMSensor()) {
    // Print values to serial monitor
    Serial.println("Sensor readings:");
    Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" °C");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
    Serial.print("Alcohol: "); Serial.print(alcoholValue); Serial.println(" ppm");
    Serial.print("PM1.0: "); Serial.print(pm10); Serial.println(" μg/m³");
    Serial.print("PM2.5: "); Serial.print(pm25); Serial.println(" μg/m³");
  }
}

bool readPMSensor() {
  // Structure for PMS sensor data
  struct pms5003data {
    uint16_t framelen;
    uint16_t pm10_standard, pm25_standard, pm100_standard;
    // Additional fields as needed
  } data;
  
  // Read data from PMS sensor
  // Implementation specific to PMS sensor model
  // ...
  
  // Assign values
  pm10 = data.pm10_standard;
  pm25 = data.pm25_standard;
  
  return true;
}

void updateFirebase() {
  // Get formatted date and time
  String formattedDate = timeClient.getFormattedDate();
  // Extract date part (YYYY-MM-DD)
  String datePart = formattedDate.substring(0, 10);
  
  // Extract and format time part (HH-MM-SS)
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();
  String timePart = String(currentHour) + "-" + 
                    (currentMinute < 10 ? "0" : "") + String(currentMinute) + "-" +
                    (currentSecond < 10 ? "0" : "") + String(currentSecond);
  
  // Format timestamp for Firebase
  String timestamp = datePart + " " + timePart;
  
  // Create JSON with sensor data
  json.clear();
  json.add("temperature", temperature);
  json.add("humidity", humidity);
  json.add("alcohol", alcoholValue);
  json.add("pm10", pm10);
  json.add("pm25", pm25);
  json.add("timestamp", timestamp);
  
  // Update Firebase
  String path = "sensor_data/" + datePart + "/" + timePart;
  if (Firebase.setJSON(firebaseData, path, json)) {
    Serial.println("Data sent to Firebase successfully");
  } else {
    Serial.println("Failed to send data to Firebase");
    Serial.println("Reason: " + firebaseData.errorReason());
  }
}
```

## Troubleshooting
### Common Issues

1. **WiFi Connection Failures**
   - Verify SSID and password
   - Check WiFi signal strength
   - Restart router if needed

2. **Sensor Reading Errors**
   - Check wiring connections
   - Verify power supply voltage
   - Ensure libraries are up to date

3. **Firebase Connection Issues**
   - Verify Firebase host and auth key
   - Check Firebase database rules
   - Ensure internet connectivity

4. **Inaccurate Time**
   - Check NTP server settings
   - Verify time zone offset
   - Ensure internet connectivity for NTP

## Maintenance
- Clean the PM sensor every 3-6 months
- Keep the enclosure clean and dust-free
- Check for loose connections periodically
- Update firmware when new features or improvements are available

## Power Options
- USB power adapter (5V/2A recommended)
- Power bank for portable operation
- 18650 lithium battery with voltage regulator
- Solar panel with battery backup for outdoor deployment

## Enclosure Guidelines
- Ensure adequate ventilation for sensors
- Position PM sensor intake away from obstructions
- Keep away from direct heat sources and water
- Mount at 1.5-2 meters height for optimal air sampling

For technical support or questions, please contact the project maintainer.

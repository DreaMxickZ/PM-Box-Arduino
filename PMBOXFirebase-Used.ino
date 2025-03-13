#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <time.h>

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// 🔹 Firebase Credentials
#define FIREBASE_HOST "" 
#define FIREBASE_AUTH ""

// 🔹 Firebase Objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// 🔹 PMS3003 Serial
SoftwareSerial pmsSerial(32, 33);  // RX, TX

// 🔹 DHT Sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// 🔹 MQ-3 Sensor
#define MQ3_PIN 34  // อ่านค่า Analog

// 🔹 OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 🔹 ตัวแปรเก็บค่าเซ็นเซอร์
float temperature, humidity, alcohol;
int pm25, pm10;
String currentDate, currentTime;

// 🔹 ตัวแปรสำหรับการจัดการเวลา
#define DISPLAY_INTERVAL 10000    // อ่านค่าและแสดงผลทุก 10 วินาที (10,000 ms)
#define UPLOAD_INTERVAL 600000    // ส่งข้อมูลไป Firebase ทุก 10 นาที (600,000 ms)
unsigned long lastDisplayTime = 0;
unsigned long lastUploadTime = 0;
int uploadCounter = 0;

// 🔹 ฟังก์ชันเชื่อม WiFi
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");
    
    // แสดงความแรงของสัญญาณ
    Serial.printf("Signal strength (RSSI): %d dBm\n", WiFi.RSSI());
    
    // รอให้การเชื่อมต่อเสถียร
    Serial.println("Waiting for stable connection...");
    delay(2000);  // รออีก 2 วินาทีหลังจากเชื่อมต่อได้แล้ว
}

// 🔹 ฟังก์ชันตั้งค่า NTP (ดึงเวลาปัจจุบัน)
void setupTime() {
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Syncing time...");
    while (time(nullptr) < 100000) {  // รอให้ได้เวลาที่ถูกต้อง
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" Time synced!");
}

// 🔹 ฟังก์ชันดึง Timestamp ปัจจุบัน
void updateTimestamp() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char dateBuffer[11], timeBuffer[9];
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeinfo);
    strftime(timeBuffer, sizeof(timeBuffer), "%H-%M-%S", &timeinfo);

    currentDate = String(dateBuffer);
    currentTime = String(timeBuffer);

    Serial.printf("Current Date: %s, Time: %s\n", currentDate.c_str(), currentTime.c_str());
}

// 🔹 ฟังก์ชันอ่านค่าจาก PMS3003
void readPMS3003() {
    if (pmsSerial.available()) {
        uint8_t buffer[32];
        pmsSerial.readBytes(buffer, 32);

        pm25 = (buffer[4] << 8) | buffer[5];
        pm10 = (buffer[6] << 8) | buffer[7];

        Serial.printf("PM2.5: %d µg/m³, PM10: %d µg/m³\n", pm25, pm10);
    }
}

// 🔹 ฟังก์ชันอ่านค่าเซ็นเซอร์ทั้งหมด
void readSensors() {
    // อ่านค่า DHT11
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    // อ่านค่า MQ-3
    alcohol = analogRead(MQ3_PIN) * (5.0 / 4095.0); // ค่าความเข้มข้นแอลกอฮอล์

    // อ่านค่า PMS3003
    readPMS3003();
}

// 🔹 ฟังก์ชันแสดงผลบน OLED
void displayData() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.setCursor(0, 0);
    display.printf("Date: %s\nTime: %s\n", currentDate.c_str(), currentTime.c_str());
    display.printf("Temp: %.1f C\nHum: %.1f%%\n", temperature, humidity);
    display.printf("Alcohol: %.2f V\n", alcohol);
    display.printf("PM2.5: %d µg/m³\nPM10: %d µg/m³", pm25, pm10);

    display.display();
}

// 🔹 ฟังก์ชันส่งข้อมูลขึ้น Firebase พร้อม retry logic
void sendToFirebase() {
    updateTimestamp();
    String path = "/sensor_data/" + currentDate + "/" + currentTime;
    
    FirebaseJson json;
    json.set("timestamp", currentDate + " " + currentTime);
    json.set("temperature", temperature);
    json.set("humidity", humidity);
    json.set("alcohol", alcohol);
    json.set("pm25", pm25);
    json.set("pm10", pm10);

    int retryCount = 0;
    int maxRetries = 3;
    int retryDelay = 1000; // เริ่มต้นที่ 1 วินาที
    
    while (retryCount < maxRetries) {
        if (Firebase.setJSON(fbdo, path.c_str(), json)) {
            Serial.println("✅ Data sent to Firebase");
            return;
        } else {
            Serial.printf("❌ Firebase Error (%d/%d): %s\n", 
                        retryCount+1, maxRetries, fbdo.errorReason().c_str());
            retryCount++;
            if (retryCount < maxRetries) {
                Serial.printf("Retrying in %d ms...\n", retryDelay);
                delay(retryDelay);
                retryDelay *= 2; // Exponential backoff
            }
        }
    }
}

// 🔹 ฟังก์ชันลบข้อมูลเก่ากว่า 1 วัน พร้อม retry logic
void cleanOldData() {
    time_t now = time(nullptr) - (24 * 3600); // 24 ชั่วโมงที่แล้ว
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char oldDate[11];
    strftime(oldDate, sizeof(oldDate), "%Y-%m-%d", &timeinfo);
    String oldPath = "/sensor_data/" + String(oldDate);

    int retryCount = 0;
    int maxRetries = 3;
    int retryDelay = 1000;
    
    while (retryCount < maxRetries) {
        if (Firebase.deleteNode(fbdo, oldPath.c_str())) {
            Serial.printf("🗑️ Deleted old data: %s\n", oldPath.c_str());
            return;
        } else {
            Serial.printf("⚠️ Failed to delete old data (%d/%d): %s\n", 
                        retryCount+1, maxRetries, fbdo.errorReason().c_str());
            retryCount++;
            if (retryCount < maxRetries) {
                Serial.printf("Retrying delete in %d ms...\n", retryDelay);
                delay(retryDelay);
                retryDelay *= 2;
            }
        }
    }
}

// 🔹 Setup
void setup() {
    Serial.begin(115200);
    pmsSerial.begin(9600);  // PMS3003
    dht.begin();

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("❌ OLED initialization failed");
        while (true);
    }
    
    connectWiFi();
    setupTime();

    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    
    // เพิ่มการตั้งค่าเพื่อแก้ไขปัญหา SSL และ timeout
    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(4096);
    fbdo.setCert(nullptr);  // ปิดการตรวจสอบใบรับรอง SSL
    Firebase.setMaxRetry(fbdo, 3);
    
    // อัพเดทเวลาและอ่านค่าเซนเซอร์ครั้งแรก
    updateTimestamp();
    readSensors();
    displayData();
    
    // เริ่มต้นค่าเวลา
    lastDisplayTime = millis();
    lastUploadTime = millis();
}

// 🔹 Loop
void loop() {
    unsigned long currentMillis = millis();
    
    // ตรวจสอบการเชื่อมต่อ WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Reconnecting...");
        connectWiFi();
    }
    
    // อ่านค่าและแสดงผลทุก 10 วินาที
    if (currentMillis - lastDisplayTime >= DISPLAY_INTERVAL) {
        Serial.println("Reading sensors and updating display...");
        updateTimestamp();
        readSensors();
        displayData();
        lastDisplayTime = currentMillis;
    }
    
    // ส่งข้อมูลไป Firebase ทุก 10 นาที
    if (currentMillis - lastUploadTime >= UPLOAD_INTERVAL) {
        Serial.println("Uploading data to Firebase...");
        sendToFirebase();
        
        // ลบข้อมูลเก่าเฉพาะทุก 6 ครั้งที่อัปโหลด (ทุก 1 ชั่วโมง)
        uploadCounter++;
        if (uploadCounter % 6 == 0) {
            Serial.println("Cleaning old data...");
            cleanOldData();
        }
        
        lastUploadTime = currentMillis;
    }
    
    // ทำให้ลูปทำงานได้เร็วขึ้น ไม่ต้องหน่วงเวลาเยอะ
    delay(100);  // หน่วงเวลาเพียงเล็กน้อยเพื่อให้ CPU ไม่ทำงานหนักเกินไป
}
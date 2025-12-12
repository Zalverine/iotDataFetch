#include "select.h"
#ifdef MAIN
#include <Arduino.h>
#include <secrets.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// Enable/disable sensors here
// #define ENABLE_BME280      // BME280 temperature, pressure, humidity, altitude
#define ENABLE_DHT11       // DHT11 temperature, humidity, heat index
#define ENABLE_SOIL_MOISTURE  // Soil moisture sensor
#define ENABLE_SOIL_TEMP      // DS18B20 soil temperature sensor

// RTDB creds
#define FARM_OWNER "Niranj"        // Farm owner name
#define NODE_NAME "/Node1"          // Node name
#define FARM_SIZE 12
void connectToWiFi();
void initializeFirebase();
void uploadSensorData(JsonDocument&);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
#define UPLOAD_INTERVAL 2000

// Variables
unsigned long lastUploadTime = 0;
bool firebaseReady = false;
bool signupOK = false;


// Sensor-specific includes
#ifdef ENABLE_BME280
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#endif

#ifdef ENABLE_DHT11
#include <DHT.h>
#endif

#ifdef ENABLE_SOIL_TEMP
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

// Pin definitions
#ifdef ENABLE_DHT11
#define DHTPIN 18
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#endif

#ifdef ENABLE_SOIL_MOISTURE
#define SOIL_MOISTURE_PIN 27
#endif

#ifdef ENABLE_SOIL_TEMP
#define ONE_WIRE_BUS 26
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature soilTempSensor(&oneWire);
#endif

#ifdef ENABLE_BME280
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;
#endif

// Function declarations
void initializeSensors();
void readSensorData(JsonDocument& doc);

#ifdef ENABLE_BME280
void readBME280(JsonDocument& doc);
#endif

#ifdef ENABLE_DHT11
void readDHT11(JsonDocument& doc);
#endif

#ifdef ENABLE_SOIL_TEMP
void readSoilTemperature(JsonDocument& doc);
#endif

#ifdef ENABLE_SOIL_MOISTURE
void readSoilMoisture(JsonDocument& doc);
#endif

void setup() {
  Serial.begin(115200);
  while(!Serial) delay(10);
  
  Serial.println(F("Multi-Sensor JSON Reader"));
  Serial.println(F("========================"));
  
  initializeSensors();

  connectToWiFi();
  
  // Initialize Firebase
  initializeFirebase();

  Serial.println();
  delay(2000);
}

void loop() {
  JsonDocument doc;
  doc["timestamp"] = millis();
  
  readSensorData(doc);
    // The doc now contains the data, call your function here to process it
    // yourFunction(doc);
  
  
  serializeJson(doc, Serial);
  Serial.println();
  if (millis() - lastUploadTime > UPLOAD_INTERVAL || lastUploadTime == 0) {
    if (firebaseReady) {
      uploadSensorData(doc);
      lastUploadTime = millis();
    } else {
      Serial.println("Firebase not ready. Retrying...");
      initializeFirebase();
    }
  }
  
  delay(2000);
}


// Connect to WiFi
void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi Connection Failed!");
    Serial.println("Please check your credentials and restart.");
  }
}

// Initialize Firebase
void initializeFirebase() {
  Serial.println("\nInitializing Firebase...");
  
  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  // Enable anonymous authentication
  auth.user.email = "";
  auth.user.password = "";

  // Signup anonymously in firebase before trying to upload or read data.
  //Can sign up anonymously a lot of times...
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signup successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase signup failed: %s\n", config.signer.signupError.message.c_str());
  }

  // Assign the callback function for token generation
  config.token_status_callback = tokenStatusCallback;
  
  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Set the size of HTTP response buffer
  fbdo.setBSSLBufferSize(1024, 1024);
  
  // Set timeout
  fbdo.setResponseSize(1024);
  
  Serial.println("Firebase initialized!");
  
  // Wait for Firebase to be ready
  int attempts = 0;
  while (!Firebase.ready() && attempts < 30) {
    Serial.print(".");
    delay(500);
    attempts++;
  }
  
  Serial.println();
  
  if (Firebase.ready()) {
    firebaseReady = true;
    Serial.println("Firebase is ready!");
  } else {
    firebaseReady = false;
    Serial.println("Firebase connection failed!");
    Serial.println("Please check your API_KEY and DATABASE_URL");
  }
}

// Initialize all enabled sensors
void initializeSensors() {
  #ifdef ENABLE_BME280
  Wire.begin();
  delay(100);
  if (!bme.begin(0x76, &Wire)) {
    Serial.println(F("BME280 initialization failed!"));
  } else {
    Serial.println(F("BME280 initialized"));
  }
  #endif
  
  #ifdef ENABLE_DHT11
  dht.begin();
  Serial.println(F("DHT11 initialized"));
  #endif
  
  #ifdef ENABLE_SOIL_TEMP
  soilTempSensor.begin();
  Serial.println(F("DS18B20 soil temperature initialized"));
  #endif
  
  #ifdef ENABLE_SOIL_MOISTURE
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  Serial.println(F("Soil moisture sensor initialized"));
  #endif
}

// Read data from all enabled sensors
void readSensorData(JsonDocument& doc) {
  #ifdef ENABLE_BME280
  readBME280(doc);
  #endif
  
  #ifdef ENABLE_DHT11
  readDHT11(doc);
  #endif
  
  #ifdef ENABLE_SOIL_TEMP
  readSoilTemperature(doc);
  #endif
  
  #ifdef ENABLE_SOIL_MOISTURE
  readSoilMoisture(doc);
  #endif
}

// Call: uploadSensorData(doc);
// Requires: fbdo (FirebaseData), Firebase initialized and firebaseReady true, ArduinoJson included

void uploadSensorData(JsonDocument& doc) {
  Serial.println("\n==========================================");
  Serial.println("Uploading sensor JSON to Firebase RTDB...");

  // Make sure Firebase is initialized on your side before calling this.
  // Build base path: <FARM_OWNER>/FarmData<NODE_NAME>
  String basePath = String(FARM_OWNER);
  basePath.concat(String("/FarmData"));
  basePath.concat(String(NODE_NAME)); // NODE_NAME may contain leading slash in your project

  // Create a timestamp key (unique). Replace with epoch if you have NTP/time available.
  unsigned long ts = millis();
  doc["uploaded_at_ms"] = ts;

  // Serialize the incoming JSON document
  String jsonString;
  serializeJson(doc, jsonString);

  // Target path for the full JSON (under lastReadings/<ts>)
  String targetPath = basePath;
  targetPath.concat(String("/lastReadings/"));
  targetPath.concat(String(ts));

  bool overallSuccess = true;

  // Create FirebaseJson object from serialized string
  FirebaseJson fbJson;
  fbJson.setJsonData(jsonString.c_str());

  // Upload full JSON at timestamped node
  if (Firebase.RTDB.setJSON(&fbdo, targetPath.c_str(), &fbJson)) {
    Serial.print("✓ JSON uploaded to: ");
    Serial.println(targetPath);
  } else {
    Serial.print("✗ JSON upload failed: ");
    Serial.println(fbdo.errorReason());
    overallSuccess = false;
  }

  // Also update 'latest' pointer with same JSON (so easy reads)
  String latestPath = basePath;
  latestPath.concat(String("/lastReadings/latest"));
  if (Firebase.RTDB.setJSON(&fbdo, latestPath.c_str(), &fbJson)) {
    Serial.println("✓ 'latest' updated");
  } else {
    Serial.print("⚠ failed to update 'latest' pointer: ");
    Serial.println(fbdo.errorReason());
    overallSuccess = false;
  }

  // ---- Upload individual scalar fields (if present in JSON) ----
  // dht11: temperature, humidity, heatIndex
  if (doc.containsKey("dht11")) {
    JsonObject dht = doc["dht11"];
    if (dht.containsKey("temperature")) {
      float temperature = dht["temperature"].as<float>();
      String path = basePath; path.concat(String("/Temperature"));
      if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), temperature)) {
        Serial.println("✓ Temperature uploaded");
      } else {
        Serial.print("✗ Temperature upload failed: ");
        Serial.println(fbdo.errorReason());
        overallSuccess = false;
      }
    }

    if (dht.containsKey("humidity")) {
      float humidity = dht["humidity"].as<float>();
      String path = basePath; path.concat(String("/Humidity"));
      if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), humidity)) {
        Serial.println("✓ Humidity uploaded");
      } else {
        Serial.print("✗ Humidity upload failed: ");
        Serial.println(fbdo.errorReason());
        overallSuccess = false;
      }
    }

    if (dht.containsKey("heatIndex")) {
      float hi = dht["heatIndex"].as<float>();
      String path = basePath; path.concat(String("/HeatIndex"));
      if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), hi)) {
        Serial.println("✓ HeatIndex uploaded");
      } else {
        Serial.print("✗ HeatIndex upload failed: ");
        Serial.println(fbdo.errorReason());
        overallSuccess = false;
      }
    }
  }

  // soilTemperature: celsius, fahrenheit
  if (doc.containsKey("soilTemperature")) {
    JsonObject st = doc["soilTemperature"];
    if (st.containsKey("celsius")) {
      float sc = st["celsius"].as<float>();
      String path = basePath; path.concat(String("/SoilTemperature"));
      if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), sc)) {
        Serial.println("✓ Soil Temperature uploaded");
      } else {
        Serial.print("✗ Soil Temperature upload failed: ");
        Serial.println(fbdo.errorReason());
        overallSuccess = false;
      }
    }
    // }
    // if (st.containsKey("fahrenheit")) {
    //   float sf = st["fahrenheit"].as<float>();
    //   String path = basePath; path.concat(String("/SoilTemperatureF"));
    //   if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), sf)) {
    //     Serial.println("✓ Soil Temperature (F) uploaded");
    //   } else {
    //     Serial.print("✗ Soil Temperature (F) upload failed: ");
    //     Serial.println(fbdo.errorReason());
    //     overallSuccess = false;
    //   }
    // }
  }

  // soilMoisture: raw, percentage
  if (doc.containsKey("soilMoisture")) {
    JsonObject sm = doc["soilMoisture"];
    if (sm.containsKey("percentage")) {
      float pct = sm["percentage"].as<float>();
      String path = basePath; path.concat(String("/SoilMoisture"));
      if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), pct)) {
        Serial.println("✓ Soil Moisture (percentage) uploaded");
      } else {
        Serial.print("✗ Soil Moisture (percentage) upload failed: ");
        Serial.println(fbdo.errorReason());
        overallSuccess = false;
      }
    }
  }

  // Summary
  if (overallSuccess) {
    Serial.println("\n✓ All data uploaded successfully!");
  } else {
    Serial.println("\n⚠ Some data failed to upload (see lines above)");
  }

  Serial.println("==========================================");
}


#ifdef ENABLE_BME280
// Read BME280 sensor (temperature, pressure, humidity, altitude)
void readBME280(JsonDocument& doc) {
  JsonObject bme280 = doc["bme280"].to<JsonObject>();
  bme280["temperature"] = round(bme.readTemperature() * 100) / 100.0;
  bme280["pressure"] = round(bme.readPressure() / 100.0F * 100) / 100.0;
  bme280["humidity"] = round(bme.readHumidity() * 100) / 100.0;
  bme280["altitude"] = round(bme.readAltitude(SEALEVELPRESSURE_HPA) * 100) / 100.0;
}
#endif

#ifdef ENABLE_DHT11
// Read DHT11 sensor (temperature, humidity, heat index)
void readDHT11(JsonDocument& doc) {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (!isnan(h) && !isnan(t)) {
    JsonObject dht11 = doc["dht11"].to<JsonObject>();
    dht11["temperature"] = round(t * 100) / 100.0;
    dht11["humidity"] = round(h * 100) / 100.0;
    dht11["heatIndex"] = round(dht.computeHeatIndex(t, h, false) * 100) / 100.0;
  } else {
    doc["dht11"] = "error";
  }
}
#endif

#ifdef ENABLE_SOIL_TEMP
// Read DS18B20 soil temperature sensor
void readSoilTemperature(JsonDocument& doc) {
  soilTempSensor.requestTemperatures();
  delay(50); // Give sensor time to read
  float soilTempC = soilTempSensor.getTempCByIndex(0);
  float soilTempF = soilTempSensor.getTempFByIndex(0);
  
  JsonObject soilTempData = doc["soilTemperature"].to<JsonObject>();
  soilTempData["celsius"] = round(soilTempC * 100) / 100.0;
  soilTempData["fahrenheit"] = round(soilTempF * 100) / 100.0;
}
#endif

#ifdef ENABLE_SOIL_MOISTURE
// Read analog soil moisture sensor
void readSoilMoisture(JsonDocument& doc) {
  int soilMoisture = analogRead(SOIL_MOISTURE_PIN);
  JsonObject soilData = doc["soilMoisture"].to<JsonObject>();
  soilData["raw"] = soilMoisture;
  soilData["percentage"] = map(soilMoisture, 0, 4095, 0, 100);
}
#endif

#endif


#include "select.h"
#ifdef FIREBASE
#include <Arduino.h>
#include "secrets.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>


// Firebase path configuration
#define FARM_OWNER "Niranj"        // Farm owner name
#define NODE_NAME "/Node1"          // Node name
#define FARM_SIZE 12              // Farm size in acres

// Sensor pin definitions
#define DHTPIN 18
#define DHTTYPE DHT11
#define SOIL_MOISTURE_PIN 27
#define ONE_WIRE_BUS 26

// Upload interval (milliseconds)
#define UPLOAD_INTERVAL 10000  // Upload every 10 seconds
// ===== END CONFIGURATION =====

// Sensor objects
DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature soilTempSensor(&oneWire);

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variables
unsigned long lastUploadTime = 0;
bool firebaseReady = false;
bool signupOK = false;

// Function declarations
void connectToWiFi();
void initializeFirebase();
void initializeSensors();
void uploadSensorData();
float readTemperature();
float readHumidity();
int readSoilMoisture();
float readSoilTemperature();

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  ESP32 Firebase IoT Data Logger");
  Serial.println("==========================================");
  
  // Initialize sensors
//   initializeSensors();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Initialize Firebase
  initializeFirebase();
  
  Serial.println("Setup complete! Starting data upload...");
  Serial.println("==========================================\n");
}

void loop() {
  // Check if it's time to upload data
  if (millis() - lastUploadTime > UPLOAD_INTERVAL || lastUploadTime == 0) {
    if (firebaseReady) {
      uploadSensorData();
      lastUploadTime = millis();
    } else {
      Serial.println("Firebase not ready. Retrying...");
      initializeFirebase();
    }
  }
  
  delay(5000);
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

// Initialize all sensors
void initializeSensors() {
  Serial.println("\nInitializing sensors...");
  
  // Initialize DHT11
  dht.begin();
  Serial.println("✓ DHT11 initialized");
  
  // Initialize soil temperature sensor
  soilTempSensor.begin();
  Serial.println("✓ DS18B20 soil temperature initialized");
  
  // Initialize soil moisture sensor
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  Serial.println("✓ Soil moisture sensor initialized");
  
  Serial.println("All sensors initialized!\n");
}

// Read DHT11 temperature
float readTemperature() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("Error reading temperature!");
    return 0.0;
  }
  return temp;
}

// Read DHT11 humidity
float readHumidity() {
  float humidity = dht.readHumidity();
  if (isnan(humidity)) {
    Serial.println("Error reading humidity!");
    return 0.0;
  }
  return humidity;
}

// Read soil moisture percentage
int readSoilMoisture() {
  int rawValue = analogRead(SOIL_MOISTURE_PIN);
  // Map the raw value (0-4095) to percentage (0-100)
  // Note: You may need to calibrate these values based on your sensor
  int percentage = map(rawValue, 0, 4095, 0, 100);
  return percentage;
}

// Read soil temperature
float readSoilTemperature() {
  soilTempSensor.requestTemperatures();
  delay(100); // Give sensor time to read
  float soilTemp = soilTempSensor.getTempCByIndex(0);
  if (soilTemp == DEVICE_DISCONNECTED_C) {
    Serial.println("Error reading soil temperature!");
    return 0.0;
  }
  return soilTemp;
}

// Upload sensor data to Firebase
void uploadSensorData() {
  Serial.println("\n==========================================");
  Serial.println("Reading sensors and uploading to Firebase...");
  
  // Read all sensor values
  float temperature = 19.86; //readTemperature()
  float humidity = 61.98; //readHumidity()
  float soilMoisture = 44.57; //readSoilMoisture()
  float soilTemp = 19.29; //readSoilTemperature()
  
  // Print sensor readings
  Serial.println("\nSensor Readings:");
  Serial.printf("  Temperature: %.1f°C\n", temperature);
  Serial.printf("  Humidity: %.0f%%\n", humidity);
  Serial.printf("  Soil Moisture: %d%%\n", soilMoisture);
  Serial.printf("  Soil Temperature: %.0f°C\n", soilTemp);
  
  // Create the base path for this node
  String basePath = String(FARM_OWNER);
  basePath.concat(String("/FarmData"));
  basePath.concat(String(NODE_NAME));
  
  // Upload each value to Firebase
  bool success = true;
  
  // Upload Temperature
  String tempPath = basePath;
  tempPath.concat(String("/Temperature"));
  if (Firebase.RTDB.setFloat(&fbdo, tempPath.c_str(), temperature)) {
    Serial.println("✓ Temperature uploaded");
  } else {
    Serial.println("✗ Temperature upload failed");
    success = false;
  }
  
  // Upload Humidity
  String humidityPath = basePath;
  humidityPath.concat(String("/Humidity"));
  if (Firebase.RTDB.setInt(&fbdo, humidityPath.c_str(), humidity)) {
    Serial.println("✓ Humidity uploaded");
  } else {
    Serial.println("✗ Humidity upload failed");
    success = false;
  }
  
  // Upload Soil Moisture
  String soilMoisturePath = basePath;
  soilMoisturePath.concat(String("/SoilMoisture"));
  if (Firebase.RTDB.setFloat(&fbdo, soilMoisturePath.c_str(), soilMoisture)) {
    Serial.println("✓ Soil Moisture uploaded");
  } else {
    Serial.println("✗ Soil Moisture upload failed:");

    success = false;
  }
  
  // Upload Soil Temperature
  String soilTempPath = basePath;
  soilTempPath.concat(String("/SoilTemp"));
  if (Firebase.RTDB.setInt(&fbdo, soilTempPath.c_str(), soilTemp)) {
    Serial.println("✓ Soil Temperature uploaded");
  } else {
    Serial.println("✗ Soil Temperature upload failed");
    success = false;
  }
  
  if (success) {
    Serial.println("\n✓ All data uploaded successfully!");
  } else {
    Serial.println("\n⚠ Some data failed to upload");
  }
  
  Serial.println("==========================================");
}

#endif
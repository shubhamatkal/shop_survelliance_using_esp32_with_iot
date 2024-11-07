#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <SPIFFS.h>
#include "config.h"

// Wi-Fi credentials
const char* ssid = WIFI_NAME;
const char* password = WIFI_PASS;

// Telegram API details
const String telegramBotToken = TELEGRAM_BOT_TOKEN; 
const String telegramChatId = TELEGRAM_GRP_CHAT_ID;

// NTP server settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; // Set to your timezone's GMT offset in seconds
const int daylightOffset_sec = 3600; // Daylight savings time offset in seconds, if applicable
const int timeUpdateInterval = 60; // Time update interval in seconds

// Sensor pins
const int pir1 = 13;
const int pir2 = 12;
const int shutter = 14;
const int drawer = 27;
const int officeDoor = 25;

// Sensor states
bool wifi_connected = false;
bool shutter_closed = false;
bool drawer_closed = false;
bool office_door_closed = false;
bool desk1_occupied = false;
bool desk2_occupied = false;

// Time variables
bool time_initialized = false;
String current_timestamp = "01/01/1001 00:00:00*";

// Persistent storage settings
const String pendingMessagesFile = "/pending_messages.txt";
const size_t maxPendingMessagesFileSize = 500 * 1024; // 500 KB

void setup();
void loop();
void connectToWifi();
void initializeTime();
void updateTime();
void readSensorStates();
void sendTelegramNotifications();
void sendTelegramMessage(String message);
String getTimeStamp();
void savePendingMessage(String message);
void sendPendingMessages();
void trimPendingMessagesFile();
void checkStatusCommand();
void sendStatusUpdate();

void setup() {
  Serial.begin(115200);
  pinMode(pir1, INPUT);
  pinMode(pir2, INPUT);
  pinMode(shutter, INPUT);
  pinMode(drawer, INPUT);
  pinMode(officeDoor, INPUT);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed");
    return;
  }

  // Connect to WiFi
  connectToWifi();

  // Initialize time using NTP
  initializeTime();

  // Check for pending messages and send them if WiFi is connected
  sendPendingMessages();
}

void loop() {
  // Check if WiFi is still connected
  if (wifi_connected && WiFi.status() != WL_CONNECTED) {
    wifi_connected = false;
    time_initialized = false;
  }

  // Update sensor states
  readSensorStates();

  // Send Telegram notifications if necessary
  sendTelegramNotifications();

  // Update time if necessary
  updateTime();

  // Check for "/1" command from Telegram
  checkStatusCommand();

  delay(100); // Adjust this delay based on your desired loop speed
}

void connectToWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  wifi_connected = true;
}

void initializeTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait for time to be set
  while (!time(nullptr)) {
      Serial.print(".");
      delay(500);
  }
  Serial.println("Time initialized successfully.");
  time_initialized = true;
  current_timestamp = getTimeStamp();
}

void updateTime() {
  if (!time_initialized || (millis() / 1000) % timeUpdateInterval == 0) {
    if (wifi_connected) {
      initializeTime();
    } else {
      current_timestamp = "01/01/1001 00:00:00*";
    }
  }
}

void readSensorStates() {
  // Read PIR sensor states
  desk1_occupied = digitalRead(pir1) == HIGH;
  desk2_occupied = digitalRead(pir2) == HIGH;

  // Read magnetic door sensor states
  shutter_closed = digitalRead(shutter) == LOW;
  drawer_closed = digitalRead(drawer) == LOW;
  office_door_closed = digitalRead(officeDoor) == LOW;
}

void sendTelegramNotifications() {
  if (wifi_connected) {
    if (shutter_closed && !shutter_closed) {
      sendTelegramMessage("Shutter is closed at " + current_timestamp);
      shutter_closed = true;
    }

    if (drawer_closed && !drawer_closed) {
      sendTelegramMessage("Drawer is closed at " + current_timestamp);
      drawer_closed = true;
    }

    if (office_door_closed && !office_door_closed) {
      sendTelegramMessage("Office door is closed at " + current_timestamp);
      office_door_closed = true;
    }

    if (!desk1_occupied && !desk2_occupied) {
      sendTelegramMessage("No employee present in the shop at " + current_timestamp);
    } else {
      if (desk1_occupied && !desk1_occupied) {
        sendTelegramMessage("Employee is present at desk 1 at " + current_timestamp);
        desk1_occupied = true;
      }
      if (desk2_occupied && !desk2_occupied) {
        sendTelegramMessage("Employee is present at desk 2 at " + current_timestamp);
        desk2_occupied = true;
      }
    }
  } else {
    // Save message to persistent storage if WiFi is not available
    savePendingMessage("Sensor event at " + current_timestamp);
  }
}

void sendTelegramMessage(String message) {
  if (wifi_connected) {
    HTTPClient http;
    http.begin("https://api.telegram.org/bot" + telegramBotToken + "/sendMessage?chat_id=" + telegramChatId + "&text=" + message);
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      Serial.println("Telegram message sent successfully");
    } else {
      Serial.print("Failed to send Telegram message, error code: ");
      Serial.println(httpResponseCode);
      // Save message to persistent storage if Telegram send failed
      savePendingMessage(message);
    }
    http.end();
  }
}

String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return current_timestamp;
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

void savePendingMessage(String message) {
  File file = SPIFFS.open(pendingMessagesFile, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open pending messages file");
    return;
  }

  file.println(message);
  file.close();

  // Check if the file size exceeds the limit and remove old messages if necessary
  trimPendingMessagesFile();
}

void sendPendingMessages() {
  if (wifi_connected) {
    File file = SPIFFS.open(pendingMessagesFile, FILE_READ);
    if (!file) {
      Serial.println("Failed to open pending messages file");
      return;
    }

    while (file.available()) {
      String message = file.readStringUntil('\n');
      sendTelegramMessage(message);
    }

    file.close();
    SPIFFS.remove(pendingMessagesFile);
  }
}

void trimPendingMessagesFile() {
  if (SPIFFS.exists(pendingMessagesFile)) {
    File file = SPIFFS.open(pendingMessagesFile, FILE_READ);
    if (!file) {
      Serial.println("Failed to open pending messages file");
      return;
    }

    size_t fileSize = file.size();
    if (fileSize > maxPendingMessagesFileSize) {
      size_t numLinesToRemove = 10; // Remove the first 10 lines
      for (size_t i = 0; i < numLinesToRemove; i++) {
        file.readStringUntil('\n');
      }

      File tempFile = SPIFFS.open("/temp.txt", FILE_WRITE);
      if (!tempFile) {
        Serial.println("Failed to create temp file");
        file.close();
        return;
      }

      while (file.available()) {
        tempFile.write(file.read());
      }

      file.close();
      tempFile.close();

      SPIFFS.remove(pendingMessagesFile);
      SPIFFS.rename("/temp.txt", pendingMessagesFile);
    } else {
      file.close();
    }
  }
}

void checkStatusCommand() {
  if (wifi_connected) {
    HTTPClient http;
    http.begin("https://api.telegram.org/bot" + telegramBotToken + "/getUpdates");
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String response = http.getString();
      if (response.indexOf("/1") != -1) {
        sendStatusUpdate();
      }
    } else {
      Serial.print("Failed to check for status command, error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void sendStatusUpdate() {
  String statusMessage = "Bharat Multiservices:\n";
  statusMessage += "1. Wifi is " + String(wifi_connected ? "connected" : "disconnected") + "\n";
  statusMessage += "2. Shop is " + String(shutter_closed ? "closed" : "open") + "\n";
  statusMessage += "3. Office door is " + String(office_door_closed ? "closed" : "open") + "\n";
  statusMessage += "4. Drawer is " + String(drawer_closed ? "closed" : "open") + "\n";
  statusMessage += "5. Employee is " + String(desk1_occupied || desk2_occupied ? "present" : "absent") + "\n";

  sendTelegramMessage(statusMessage);
}
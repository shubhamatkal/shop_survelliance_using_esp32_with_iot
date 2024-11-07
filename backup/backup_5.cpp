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
// const long gmtOffset_sec = 3600;
// const int daylightOffset_sec = 3600;
// const int timeUpdateInterval = 60;
const long gmtOffset_sec = 19800;    // Changed to 5 hours 30 minutes (5*3600 + 30*60)
const int daylightOffset_sec = 0;    // India doesn't use daylight saving
const int timeUpdateInterval = 1;    // Changed to 1 second from 60

// Sensor pins
const int pir1 = 13;
const int pir2 = 12;
const int shutter = 14;
const int drawer = 27;
const int officeDoor = 25;

// Current sensor states
bool wifi_connected = false;
bool shutter_closed = false;
bool drawer_closed = false;
bool office_door_closed = false;
bool desk1_occupied = false;
bool desk2_occupied = false;

// Previous sensor states for change detection
bool prev_shutter_closed = false;
bool prev_drawer_closed = false;
bool prev_office_door_closed = false;
bool prev_desk1_occupied = false;
bool prev_desk2_occupied = false;
bool prev_any_desk_occupied = false;

// Time variables
bool time_initialized = false;
String current_timestamp = "01/01/1001 00:00:00*";

// Persistent storage settings
const String pendingMessagesFile = "/pending_messages.txt";
const size_t maxPendingMessagesFileSize = 500 * 1024; // 500 KB

// Function prototypes - declare all functions before setup()
void connectToWifi();
void initializeTime();
void updateTime();
void readSensorStates();
void processSensorChanges();
void sendTelegramMessage(String message);
String getTimeStamp();
void savePendingMessage(String message);
void sendPendingMessages();
void trimPendingMessagesFile();
void checkStatusCommand();
void sendStatusUpdate();
String urlEncode(String str);

void setup() {
    Serial.begin(115200);
    
    // Set pin modes with internal pullup resistors
    pinMode(pir1, INPUT);
    pinMode(pir2, INPUT);
    pinMode(shutter, INPUT_PULLUP);
    pinMode(drawer, INPUT_PULLUP);
    pinMode(officeDoor, INPUT_PULLUP);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed");
        return;
    }

    // // // Delete the pending messages file
    // if(SPIFFS.exists(pendingMessagesFile)) {
    //     SPIFFS.remove(pendingMessagesFile);
    //     Serial.println("Deleted old pending messages file");
    // }

    connectToWifi();
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + telegramBotToken + "/getUpdates?offset=-1";
    http.begin(url);
    http.GET();
    http.end();
    initializeTime();
    
    // Initialize previous states
    readSensorStates();
    prev_shutter_closed = shutter_closed;
    prev_drawer_closed = drawer_closed;
    prev_office_door_closed = office_door_closed;
    prev_desk1_occupied = desk1_occupied;
    prev_desk2_occupied = desk2_occupied;
    prev_any_desk_occupied = desk1_occupied || desk2_occupied;

    sendPendingMessages();
}

void loop() {
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        if (wifi_connected) {
            wifi_connected = false;
            time_initialized = false;
            Serial.println("WiFi disconnected");
        }
        connectToWifi();
    }

    // Update time if necessary
    updateTime();

    // Read and process sensor states
    readSensorStates();
    processSensorChanges();

    // Check for status command every 5 seconds
    static unsigned long lastCheckTime = 0;
    if (millis() - lastCheckTime >= 5000) {
        checkStatusCommand();
        lastCheckTime = millis();
    }

    delay(100);
}

void connectToWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        return;
    }

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 2) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.println("IP address: " + WiFi.localIP().toString());
        wifi_connected = true;
        sendTelegramMessage("ESP32 successfully connected to WiFi");
        sendPendingMessages();
    } else {
        Serial.println("\nWiFi connection failed");
        wifi_connected = false;
    }
}

void initializeTime() {
    if (!wifi_connected) {
        Serial.println("Cannot initialize time: WiFi not connected");
        return;
    }

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    int attempts = 0;
    while (!time(nullptr) && attempts < 10) {
        Serial.print(".");
        delay(500);
        attempts++;
    }

    if (time(nullptr)) {
        Serial.println("\nTime initialized successfully");
        time_initialized = true;
        current_timestamp = getTimeStamp();
    } else {
        Serial.println("\nTime initialization failed");
        time_initialized = false;
    }
}

void updateTime() {
    static unsigned long lastTimeUpdate = 0;
    
    if (!time_initialized || (millis() - lastTimeUpdate >= timeUpdateInterval * 1000)) {
        if (wifi_connected) {
            current_timestamp = getTimeStamp();
            lastTimeUpdate = millis();
        } else {
            current_timestamp = "01/01/1001 00:00:00*";
        }
    }
}

void readSensorStates() {
    // Store previous states
    prev_shutter_closed = shutter_closed;
    prev_drawer_closed = drawer_closed;
    prev_office_door_closed = office_door_closed;
    prev_desk1_occupied = desk1_occupied;
    prev_desk2_occupied = desk2_occupied;
    prev_any_desk_occupied = desk1_occupied || desk2_occupied;

    // Read current states
    // Note: LOW means closed for magnetic sensors due to pull-up resistors
    shutter_closed = digitalRead(shutter) == LOW;
    drawer_closed = digitalRead(drawer) == LOW;
    office_door_closed = digitalRead(officeDoor) == LOW;
    
    // PIR sensors: HIGH means motion detected
    desk1_occupied = digitalRead(pir1) == HIGH;
    desk2_occupied = digitalRead(pir2) == HIGH;
}

void processSensorChanges() {
    // Process state changes and send notifications
    if (shutter_closed != prev_shutter_closed) {
        sendTelegramMessage("Shutter is " + String(shutter_closed ? "closed" : "open") + " at " + current_timestamp);
    }

    if (drawer_closed != prev_drawer_closed) {
        sendTelegramMessage("Drawer is " + String(drawer_closed ? "closed" : "open") + " at " + current_timestamp);
    }

    if (office_door_closed != prev_office_door_closed) {
        sendTelegramMessage("Office door is " + String(office_door_closed ? "closed" : "open") + " at " + current_timestamp);
    }

    // Check for occupancy changes
    bool current_any_desk_occupied = desk1_occupied || desk2_occupied;
    if (current_any_desk_occupied != prev_any_desk_occupied) {
        if (!current_any_desk_occupied) {
            sendTelegramMessage("No employee present in the shop at " + current_timestamp);
        }
    }

    if (desk1_occupied != prev_desk1_occupied && desk1_occupied) {
        sendTelegramMessage("Employee is present at desk 1 at " + current_timestamp);
    }

    if (desk2_occupied != prev_desk2_occupied && desk2_occupied) {
        sendTelegramMessage("Employee is present at desk 2 at " + current_timestamp);
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

void sendTelegramMessage(String message) {
    if (!wifi_connected) {
        savePendingMessage(message);
        return;
    }

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + telegramBotToken + "/sendMessage";
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String postData = "chat_id=" + telegramChatId + "&text=" + urlEncode(message);
    int httpResponseCode = http.POST(postData);
    
    if (httpResponseCode != 200) {
        Serial.printf("Failed to send Telegram message, error code: %d\n", httpResponseCode);
        savePendingMessage(message);
    } else {
        Serial.println("Telegram message sent successfully");
    }
    http.end();
}

void savePendingMessage(String message) {
    // Open the file in FILE_WRITE mode to create it if it doesn’t exist
    File file = SPIFFS.open(pendingMessagesFile, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open or create pending messages file");
        return;
    }

    // Append the message
    file.println(message);
    file.close();

    // Trim excess messages if needed
    trimPendingMessagesFile();
}

void sendPendingMessages() {
    sendTelegramMessage("Inside sendPendingMessages");
    if (!wifi_connected || !SPIFFS.exists(pendingMessagesFile)) {
        return;
    }

    File file = SPIFFS.open(pendingMessagesFile, FILE_READ);
    if (!file) {
        Serial.println("Failed to open pending messages file");
        sendTelegramMessage("Failed to open pending messages file");
        return;
    }

    // Check if the file is empty
    if (file.size() == 0) {
        file.close();  // Close the file if it's empty
        sendTelegramMessage("File is empty");
        return;        // Exit the function immediately
    }

    // String unsentMessages = ""; // Buffer for unsent messages
    // bool allMessagesSent = true;

    // // Read each line and attempt to send it
    // while (file.available()) {
    //     String message = file.readStringUntil('\n');
    //     message.trim();

    // Read each line and attempt to send it using sendTelegrammsg

    String allMessages; // Buffer for all messages

    // Read the entire file content
    while (file.available()) {
        String message = file.readStringUntil('\n');
        allMessages += message + "\n"; // Append each message to allMessages
    }

    file.close(); // Close the file after reading

    if (allMessages.length() > 0) {
        sendTelegramMessage(allMessages); // Send all messages at once
    }

    // while (file.available()) {
    //     String message = file.readStringUntil('\n');
    //     message.trim();

    //     if (message.length() > 0) {
    //         sendTelegramMessage(message);
    //         // if (!sendTelegramMessage(message)) {
    //         //     // If sending failed, add to unsentMessages buffer
    //         //     allMessagesSent = false;
    //         //     unsentMessages += message + "\n";
    //         // }
    //     }
    // }
    

    // file.close();

    // If there are unsent messages, overwrite the file with them
    // if (!allMessagesSent) {
    //     file = SPIFFS.open(pendingMessagesFile, FILE_WRITE);
    //     if (!file) {
    //         Serial.println("Failed to open pending messages file for overwriting");
    //         return;
    //     }

    //     file.print(unsentMessages);
    //     file.close();
    // } else {
        // If all messages were sent, remove the file
    SPIFFS.remove(pendingMessagesFile);
    sendTelegramMessage("File removed and sent all the pending msgs done with send pending messages");
    // }
}


// void sendPendingMessages() {
//     if (!wifi_connected || !SPIFFS.exists(pendingMessagesFile)) {
//         return;
//     }

//     File file = SPIFFS.open(pendingMessagesFile, FILE_READ);
//     if (!file) {
//         Serial.println("Failed to open pending messages file");
//         return;
//     }

//     bool allMessagesSent = true;
//     String pendingMessages = "";

//     while (file.available()) {
//         String message = file.readStringUntil('\n');
//         message.trim();
        
//         if (message.length() > 0) {
//             HTTPClient http;
//             String url = "https://api.telegram.org/bot" + telegramBotToken + "/sendMessage";
//             http.begin(url);
//             http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            
//             String postData = "chat_id=" + telegramChatId + "&text=" + urlEncode(message);
//             int httpResponseCode = http.POST(postData);
            
//             if (httpResponseCode != 200) {
//                 allMessagesSent = false;
//                 pendingMessages += message + "\n";
//             }
            
//             http.end();
//             delay(50); // Prevent flooding Telegram API

//         }
//     }

//     file.close();
//     SPIFFS.remove(pendingMessagesFile);

//     if (!allMessagesSent) {
//         File newFile = SPIFFS.open(pendingMessagesFile, FILE_WRITE);
//         if (newFile) {
//             newFile.print(pendingMessages);
//             newFile.close();
//         }
//     }
// }

// void trimPendingMessagesFile() {
//     if (!SPIFFS.exists(pendingMessagesFile)) {
//         return;
//     }

//     File file = SPIFFS.open(pendingMessagesFile, FILE_READ);
//     if (!file) {
//         return;
//     }

//     size_t fileSize = file.size();
//     if (fileSize <= maxPendingMessagesFileSize) {
//         file.close();
//         return;
//     }

//     // Read all lines except the oldest ones
//     String contents = "";
//     bool skippingLines = true;
//     size_t currentSize = 0;
    
//     while (file.available()) {
//         String line = file.readStringUntil('\n') + '\n';
//         if (skippingLines) {
//             if (fileSize - currentSize <= maxPendingMessagesFileSize) {
//                 skippingLines = false;
//                 contents += line;
//             }
//         } else {
//             contents += line;
//         }
//         currentSize += line.length();
//     }

//     file.close();

//     // Write back trimmed contents
//     File newFile = SPIFFS.open(pendingMessagesFile, FILE_WRITE);
//     if (newFile) {
//         newFile.print(contents);
//         newFile.close();
//     }
// }
void trimPendingMessagesFile() {
    // First, count the lines in the file
    File readFile = SPIFFS.open(pendingMessagesFile, FILE_READ);
    if (!readFile) {
        Serial.println("Failed to open file for reading");
        return;
    }

    // Count lines
    int lineCount = 0;
    while (readFile.available()) {
        if (readFile.read() == '\n') {
            lineCount++;
        }
    }
    readFile.close();

    // If lines are less than 100, no need to trim
    if (lineCount < 50) {
        return;
    }

    // Open the file for reading and writing
    File file = SPIFFS.open(pendingMessagesFile, FILE_READ);
    if (!file) {
        Serial.println("Failed to open pending messages file");
        return;
    }

    // Read all lines from the file into a String array
    String lines[50];
    int lineCount_ = 0;

    while (file.available() && lineCount_ < 50) {
        lines[lineCount_++] = file.readStringUntil('\n');
    }
    file.close();

    if (lineCount > 50) {
        // Re-open the file in FILE_WRITE mode to clear its contents
        file = SPIFFS.open(pendingMessagesFile, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open pending messages file for writing");
            return;
        }

        // Write the newest 90 lines back to the cleared file
        for (int i = 10; i < lineCount; i++) { // Skip the oldest 10 lines
            file.println(lines[i]);
        }
        file.close();
    }

}

void checkStatusCommand() {
    if (!wifi_connected) {
        return;
    }

    static unsigned long lastProcessedTime = 0;
    
    HTTPClient http;
    // Add offset and timeout parameters to get only new messages
    String url = "https://api.telegram.org/bot" + telegramBotToken + "/getUpdates?offset=-1&timeout=1";
    http.begin(url);
    
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
        String response = http.getString();
        
        // Look for a message timestamp
        int dateStart = response.indexOf("\"date\":");
        if (dateStart != -1) {
            dateStart += 7; // Move past "date":
            int dateEnd = response.indexOf(",", dateStart);
            unsigned long messageTime = response.substring(dateStart, dateEnd).toInt();
            
            // Find the message text
            int textStart = response.indexOf("\"text\":\"");
            if (textStart != -1) {
                textStart += 8; // Move past "text":"
                int textEnd = response.indexOf("\"", textStart);
                String messageText = response.substring(textStart, textEnd);
                
                // Only process if this is a new message and contains the command
                if (messageTime > lastProcessedTime && 
                    (messageText == "1" || messageText == "status")) {
                    lastProcessedTime = messageTime;
                    sendStatusUpdate();
                }
            }
        }
    }
    
    http.end();
}

void sendStatusUpdate() {
    String statusMessage = "Bharat Multiservices Status:\n\n";
    statusMessage += "1. WiFi : " + String(wifi_connected ? "Connected" : "Disconnected") + "\n";
    statusMessage += "2. Shop: " + String(shutter_closed ? "Closed" : "Open") + "\n";
    statusMessage += "3. Office Door: " + String(office_door_closed ? "Closed" : "Open") + "\n";
    statusMessage += "4. Drawer: " + String(drawer_closed ? "Closed" : "Open") + "\n";
    statusMessage += "5. Desk 1: " + String(desk1_occupied ? "Occupied" : "Vacant") + "\n";
    statusMessage += "6. Desk 2: " + String(desk2_occupied ? "Occupied" : "Vacant") + "\n";
    statusMessage += "7. Time: " + current_timestamp;

    sendTelegramMessage(statusMessage);
}


String urlEncode(String str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;

    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum(c)) {
            encodedString += c;
        } else if (c == ' ') {
            encodedString += '+';
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
    }
    
    return encodedString;
}
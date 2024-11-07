#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

// Wi-Fi credentials
const char* ssid = WIFI_NAME;
const char* password = WIFI_PASS;

// Telegram API details
const String telegramBotToken = TELEGRAM_BOT_TOKEN; 
const String telegramChatId = TELEGRAM_GRP_CHAT_ID;

// PIR sensor pin
const int pirPin = 13; // GPIO pin connected to PIR sensor OUT
int pirState = LOW; // Initial state: no motion detected

// Initialize the client globally
WiFiClientSecure client;

// Function prototype for sendTelegramMessage
void sendTelegramMessage(const String& message);

// Function prototype for URL encode
String urlEncode(const String &str);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Configure PIR sensor pin as input
    pinMode(pirPin, INPUT);

    // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Send message to Telegram bot upon successful connection
    sendTelegramMessage("ESP32 successfully connected to Wi-Fi and ready to detect motion.");
}

void loop() {
    int motionDetected = digitalRead(pirPin);
    if (motionDetected == HIGH && pirState == LOW) {
        // Motion detected for the first time
        Serial.println("Motion detected!");
        sendTelegramMessage("Motion detected!");
        pirState = HIGH; // Update the state to indicate motion
    } else if (motionDetected == HIGH && pirState == HIGH) {
        // Motion detected for the first time
        Serial.println("Motion detected!");
        sendTelegramMessage("Motion detected!");
        pirState = HIGH; // Update the state to indicate motion
    } else if (motionDetected == LOW && pirState == HIGH) {
        // No motion detected
        Serial.println("No motion detected.");
        sendTelegramMessage("No motion detected.");
        pirState = LOW; // Update the state to indicate no motion
    } else if (motionDetected == LOW && pirState == LOW) {
    // No motion detected
    Serial.println("No motion detected.");
    sendTelegramMessage("No motion detected.");
    pirState = LOW; // Update the state to indicate no motion
}
    delay(30000); // 30-second delay between checks
}

// Helper function to URL-encode the message
String urlEncode(const String &str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encodedString += "%20";
        } else if (isalnum(c)) {
            encodedString += c;
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

// Function to send message to Telegram bot
void sendTelegramMessage(const String& message) {
    if (WiFi.status() == WL_CONNECTED) {
        client.setInsecure(); // Bypass certificate validation for Telegram
        HTTPClient https;
        String encodedMessage = urlEncode(message); // Encode message manually
        String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chat_id + "&text=" + encodedMessage;

        https.begin(client, url);
        int httpCode = https.GET();

        if (httpCode > 0) {
            Serial.printf("Message sent, response code: %d\n", httpCode);
        } else {
            Serial.printf("Failed to send message, error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
    } else {
        Serial.println("Wi-Fi not connected.");
    }
}

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Wi-Fi credentials
const char* ssid = "realme";
const char* password = "shubhamatkall";

// Telegram Bot Token
const String botToken = "8071340273:AAHCDClqDfpq2CZUv3oQpJl2LE6yU0JXPNg";  // Replace with your actual bot token
const String chat_id = "7179601736";

// Initialize the client globally
WiFiClientSecure client;

// Prototype for sendTelegramMessage function
void sendTelegramMessage(const String& message);

void setup() {
  Serial.begin(115200);
  delay(1000);

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

  // Send message to Telegram bot
  sendTelegramMessage("ESP32 successfully connected to Wi-Fi!");
}

void loop() {
  // Reconnect to Wi-Fi if connection is lost
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nReconnected to Wi-Fi!");
    // Send message to Telegram bot
    sendTelegramMessage("ESP32 successfully re-connected to Wi-Fi!");
  }
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
    client.setInsecure();  // Bypass certificate validation for Telegram

    HTTPClient https;
    String encodedMessage = urlEncode(message);  // Encode message manually
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

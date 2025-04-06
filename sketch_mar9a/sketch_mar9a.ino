#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
// #include <Arduino_JSON.h>

// WiFi credentials
const char* ssid = "Asish Maharana";
const char* password = "asish2003@@";

// Ollama API settings
const char* server = "192.168.213.114";  // Change this to your Ollama server IP if not running on the same machine
const int port = 11434;
const char* endpoint = "/api/chat";

// Model configuration
const char* model = "llama3.2";
const bool stream = false;

// Buffer to store assistant's previous response
String previousAssistantContent = "";

// WiFi client for making HTTP requests
WiFiClient client;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }

  // Connect to WiFi
  connectToWiFi();
  
  Serial.println("\n=== Ollama Chat Interface ===");
  Serial.println("Type your message and press Enter to send it to the AI");
  Serial.println("The conversation context will be maintained between messages");
}

void loop() {
  // Check if there's input available from the serial monitor
  if (Serial.available()) {
    // Read the user's input
    String userInput = Serial.readStringUntil('\n');
    userInput.trim();
    
    if (userInput.length() > 0) {
      Serial.println("\nYou: " + userInput);
      
      // Create and send the chat request
      String response = sendChatRequest(userInput);
      
      // Print the response
      Serial.println("\nAI: " + response);
      
      // Update the previous assistant content for context
      previousAssistantContent = response;
    }
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

String sendChatRequest(String userMessage) {
  // Create JSON payload
  DynamicJsonDocument doc(8192);  // Adjust size based on your needs
  
  doc["model"] = model;
  doc["stream"] = stream;
  
  JsonArray messages = doc.createNestedArray("messages");
  
  // System message
  JsonObject systemMsg = messages.createNestedObject();
  systemMsg["role"] = "system";
  systemMsg["content"] = "You are JARVIS, a highly intelligent assistant. Answer all questions in no more than 100 words and remember the context from the previous exchange.";
  
  // Include previous assistant message if available
  if (previousAssistantContent.length() > 0) {
    JsonObject prevAssistantMsg = messages.createNestedObject();
    prevAssistantMsg["role"] = "assistant";
    prevAssistantMsg["content"] = previousAssistantContent;
  }
  
  // User message
  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  userMsg["content"] = userMessage;
  
  // Serialize JSON to string
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  Serial.println("Sending request to Ollama...");
  
  // Make HTTP POST request
  if (client.connect(server, port)) {
    // Send HTTP headers
    client.println("POST " + String(endpoint) + " HTTP/1.1");
    client.println("Host: " + String(server) + ":" + String(port));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(jsonPayload.length()));
    client.println("Connection: close");
    client.println();
    
    // Send request body
    client.println(jsonPayload);
    
    // Wait for server response
    unsigned long timeout = millis();
    while (client.connected() && !client.available()) {
      if (millis() - timeout > 10000) {  // 10 second timeout
        Serial.println(">>> Client Timeout !");
        client.stop();
        return "Error: Request timeout";
      }
      delay(50);
    }
    
    // Process response
    String response = "";
    bool isBody = false;
    
    // Read the entire response
    while (client.available()) {
      String line = client.readStringUntil('\n');
      
      // Skip headers until we reach the JSON body
      if (!isBody && line.length() <= 1) {
        isBody = true;
        continue;
      }
      
      if (isBody) {
        response += line;
      }
    }
    
    // Close connection
    client.stop();
    
    // Parse the JSON response
    DynamicJsonDocument respDoc(8192);
    DeserializationError error = deserializeJson(respDoc, response);
    
    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      return "Error parsing response";
    }
    
    // Extract the assistant's message content
    if (respDoc.containsKey("message") && respDoc["message"].containsKey("content")) {
      return respDoc["message"]["content"].as<String>();
    } else {
      return "Error: Unexpected response format";
    }
  } else {
    Serial.println("Connection failed");
    return "Error: Could not connect to the server";
  }
}
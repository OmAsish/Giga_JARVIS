#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include <lvgl.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "Asish Maharana";
const char* password = "asish2003@@";

// Ollama API settings
const char* server = "192.168.118.114";  // Replace with your PC's IP address
const int port = 11434;
const char* endpoint = "/api/chat";
const char* model = "llama3.2";
const bool stream = false;

// Global variables
Arduino_H7_Video display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch touch;
lv_obj_t* keyboard;
lv_obj_t* textarea;
static lv_style_t style_cursor;
WiFiClient client;
String previousAssistantContent = "";

// Function to connect to Wi-Fi
void connectToWiFi() {
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function to send chat request to Ollama
String sendChatRequest(String userMessage) {
  DynamicJsonDocument doc(8192);
  doc["model"] = model;
  doc["stream"] = stream;
  JsonArray messages = doc.createNestedArray("messages");

  JsonObject systemMsg = messages.createNestedObject();
  systemMsg["role"] = "system";
  systemMsg["content"] = "You are JARVIS, a highly intelligent assistant. Answer all questions in no more than 100 words and remember the context from the previous exchange.";

  if (previousAssistantContent.length() > 0) {
    JsonObject prevAssistantMsg = messages.createNestedObject();
    prevAssistantMsg["role"] = "assistant";
    prevAssistantMsg["content"] = previousAssistantContent;
  }

  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  userMsg["content"] = userMessage;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  if (client.connect(server, port)) {
    client.println("POST " + String(endpoint) + " HTTP/1.1");
    client.println("Host: " + String(server) + ":" + String(port));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(jsonPayload.length()));
    client.println("Connection: close");
    client.println();
    client.println(jsonPayload);

    unsigned long timeout = millis();
    while (client.connected() && !client.available()) {
      if (millis() - timeout > 10000) {
        client.stop();
        return "Error: Request timeout";
      }
      delay(50);
    }

    String response = "";
    bool isBody = false;
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (!isBody && line.length() <= 1) {
        isBody = true;
        continue;
      }
      if (isBody) {
        response += line;
      }
    }
    client.stop();

    DynamicJsonDocument respDoc(8192);
    DeserializationError error = deserializeJson(respDoc, response);
    if (error) {
      return "Error parsing response";
    }
    if (respDoc.containsKey("message") && respDoc["message"].containsKey("content")) {
      return respDoc["message"]["content"].as<String>();
    } else {
      return "Error: Unexpected response format";
    }
  } else {
    return "Error: Could not connect to the server";
  }
}

// Keyboard event callback
static void keyboard_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char* txt = lv_textarea_get_text(textarea);
    Serial.println(txt);
    String response = sendChatRequest(String(txt));
    previousAssistantContent = response;
    lv_textarea_set_text(textarea, response.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  display.begin();
  touch.begin();
  lv_init();
  connectToWiFi();

  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

  textarea = lv_textarea_create(scr);
  lv_obj_set_size(textarea, 800, 100);
  lv_obj_align(textarea, LV_ALIGN_TOP_MID, 0, 10);
  lv_textarea_set_placeholder_text(textarea, "Type here...");
  lv_textarea_set_cursor_click_pos(textarea, true);
  lv_obj_set_style_bg_color(textarea, lv_color_hex(0x2E2E2E), LV_PART_MAIN);
  lv_obj_set_style_border_color(textarea, lv_color_hex(0xA9A9A9), LV_PART_MAIN);
  lv_obj_set_style_border_width(textarea, 2, LV_PART_MAIN);
  lv_obj_set_style_text_color(textarea, lv_color_white(), LV_PART_MAIN);

  lv_style_init(&style_cursor);
  lv_style_set_bg_color(&style_cursor, lv_color_white());
  lv_style_set_bg_opa(&style_cursor, LV_OPA_COVER);
  lv_obj_add_style(textarea, &style_cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);

  keyboard = lv_keyboard_create(scr);
  lv_obj_set_size(keyboard, 800, 240);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_keyboard_set_textarea(keyboard, textarea);
  lv_obj_set_style_bg_color(keyboard, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x2E2E2E), LV_PART_ITEMS);
  lv_obj_set_style_text_color(keyboard, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xA9A9A9), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(keyboard, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
}

void loop() {
  lv_timer_handler();
  delay(5);
}

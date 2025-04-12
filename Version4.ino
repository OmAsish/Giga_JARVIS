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

// Global state
String previousAssistantContent = "";
WiFiClient client;

Arduino_H7_Video display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch touch;

// LVGL objects
lv_obj_t* textarea;
lv_obj_t* keyboard;
lv_obj_t* output_area;

String sendChatRequest(String userMessage) {
  DynamicJsonDocument doc(8192);
  doc["model"] = model;
  doc["stream"] = stream;

  JsonArray messages = doc.createNestedArray("messages");

  JsonObject systemMsg = messages.createNestedObject();
  systemMsg["role"] = "system";
  systemMsg["content"] = "You are JARVIS, a highly intelligent assistant of Toney, who is the user. Keep answers under 100 words.";

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
    client.println("Connection: close\n");
    client.println(jsonPayload);

    unsigned long timeout = millis();
    while (client.connected() && !client.available()) {
      if (millis() - timeout > 10000) {
        client.stop();
        return "Error: Timeout";
      }
      delay(10);
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
    if (error) return "Error: JSON Parse";

    if (respDoc.containsKey("message") && respDoc["message"].containsKey("content")) {
      return respDoc["message"]["content"].as<String>();
    } else {
      return "Error: Unexpected format";
    }
  } else {
    return "Error: Connect failed";
  }
}

void keyboard_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char* input = lv_textarea_get_text(textarea);
    String userInput = String(input);
    if (userInput.length() > 0) {
      Serial.print("User: ");
      Serial.println(userInput);

      String response = sendChatRequest(userInput);
      previousAssistantContent = response;

      Serial.print("AI: ");
      Serial.println(response);

      lv_textarea_set_text(output_area, response.c_str());
      lv_textarea_set_cursor_pos(output_area, LV_TEXTAREA_CURSOR_LAST);
    }
    // Do not clear the textarea automatically
  }
}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
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

  // Textarea for input
  textarea = lv_textarea_create(scr);
  lv_obj_set_size(textarea, 800, 60);
  lv_obj_align(textarea, LV_ALIGN_TOP_MID, 0, 10);
  lv_textarea_set_placeholder_text(textarea, "Type here...");
  lv_textarea_set_cursor_click_pos(textarea, true);
  lv_obj_set_style_bg_color(textarea, lv_color_hex(0x2E2E2E), LV_PART_MAIN);
  lv_obj_set_style_border_color(textarea, lv_color_hex(0xA9A9A9), LV_PART_MAIN);
  lv_obj_set_style_border_width(textarea, 2, LV_PART_MAIN);
  lv_obj_set_style_text_color(textarea, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_color(textarea, lv_color_white(), LV_PART_CURSOR); // white cursor

  // Output area
  output_area = lv_textarea_create(scr);
  lv_obj_set_size(output_area, 800, 120);
  lv_obj_align(output_area, LV_ALIGN_TOP_MID, 0, 80);
  lv_textarea_set_text(output_area, "");
  lv_textarea_set_cursor_click_pos(output_area, false);
  lv_textarea_set_max_length(output_area, 2048);
  lv_textarea_set_text_selection(output_area, false);
  lv_obj_set_style_bg_color(output_area, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_width(output_area, 0, LV_PART_MAIN);
  lv_obj_set_style_text_color(output_area, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_clear_flag(output_area, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(output_area, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(output_area, LV_SCROLLBAR_MODE_AUTO);

  // Keyboard
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

  // Clear Button for textarea
  lv_obj_t* clear_btn = lv_btn_create(scr);
  lv_obj_align(clear_btn, LV_ALIGN_TOP_RIGHT, -10, 20);
  lv_obj_set_size(clear_btn, 80, 40);
  lv_obj_t* label = lv_label_create(clear_btn);
  lv_label_set_text(label, "Clear");
  lv_obj_center(label);
  lv_obj_add_event_cb(clear_btn, [](lv_event_t* e) {
    lv_textarea_set_text(textarea, "");
  }, LV_EVENT_CLICKED, NULL);
}

void loop() {
  lv_timer_handler();
  delay(5);
}

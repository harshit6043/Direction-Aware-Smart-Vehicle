
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

#include "secrets.h"

// ---------------- CONFIGURATION ----------------
// These are EXAMPLE pins, not confirmed wiring.
// Change them to match your actual board and modules.
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C

#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 27

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1
);

AudioGeneratorWAV *wav = nullptr;
AudioFileSourceHTTPStream *audioFile = nullptr;
AudioOutputI2S *audioOut = nullptr;

String lastReply = "Starting...";
float headingDegrees = -1.0f; // -1 means compass not configured

// ---------------- OLED ----------------
void showStatus(const String &status, const String &message = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Direction-Aware Vehicle");
  display.println("-----------------------");

  display.print("Heading: ");
  if (headingDegrees >= 0) {
    display.print(headingDegrees, 1);
    display.println(" deg");
  } else {
    display.println("Unavailable");
  }

  display.print("Status: ");
  display.println(status);
  display.println();

  // Display a short portion of the response.
  display.println(message.substring(0, 80));
  display.display();
}

// ---------------- WIFI ----------------
bool connectWiFi() {
  showStatus("Connecting WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < 20000) {
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    showStatus("WiFi connected", WiFi.localIP().toString());
    return true;
  }

  showStatus("WiFi failed");
  return false;
}

// ---------------- GEMINI ----------------
String askGemini(float heading) {
  if (WiFi.status() != WL_CONNECTED) {
    return "WiFi unavailable.";
  }

  WiFiClientSecure client;

  // DEVELOPMENT ONLY:
  // This skips server certificate validation.
  // For deployment, configure proper certificate validation.
  client.setInsecure();

  HTTPClient https;
  String url =
    "https://generativelanguage.googleapis.com/v1beta/models/" +
    String(GEMINI_MODEL) + ":generateContent";

  if (!https.begin(client, url)) {
    return "Could not start HTTPS.";
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("x-goog-api-key", GEMINI_API_KEY);
  https.setTimeout(20000);

  String prompt;
  if (heading >= 0) {
    prompt =
      "You are a helpful direction-aware vehicle assistant. "
      "The compass heading is " + String(heading, 1) +
      " degrees clockwise from north. Give one short, safe "
      "orientation suggestion. Do not claim GPS location, "
      "route knowledge, or obstacle detection.";
  } else {
    prompt =
      "You are a helpful vehicle assistant. The compass sensor "
      "is not configured, so the heading is unknown. Explain "
      "briefly that direction guidance is unavailable until "
      "the compass is connected and configured.";
  }

  JsonDocument request;
  request["contents"][0]["parts"][0]["text"] = prompt;
  request["generationConfig"]["maxOutputTokens"] = 100;

  String body;
  serializeJson(request, body);

  int code = https.POST(body);
  String response = https.getString();
  https.end();

  if (code < 200 || code >= 300) {
    Serial.printf("Gemini HTTP error: %d\n", code);
    Serial.println(response);
    return "Gemini request failed.";
  }

  JsonDocument result;
  DeserializationError err = deserializeJson(result, response);

  if (err) {
    return "Could not parse Gemini response.";
  }

  const char *text =
    result["candidates"][0]["content"]["parts"][0]["text"];

  if (text == nullptr) {
    return "Gemini returned no text.";
  }

  return String(text);
}

// ---------------- TTS ----------------
// Requires a real HTTP endpoint that returns WAV audio.
// Leave TTS_WAV_URL_BASE empty until you configure one.
// The endpoint contract must match your chosen TTS provider.
void speakText(const String &text) {
  if (String(TTS_WAV_URL_BASE).length() == 0) {
    Serial.println("TTS not configured; text only:");
    Serial.println(text);
    return;
  }

  // A provider-specific adapter is required here to safely
  // encode text and construct its request URL.
  // Do not assume every TTS provider accepts ?text=...
  Serial.println(
    "TTS endpoint configured, but provider adapter is required."
  );
  Serial.println(text);
}

// ---------------- COMPASS ----------------
bool updateCompassHeading() {
  // NOT IMPLEMENTED YET:
  // QMC5883L, HMC5883L, and other compass modules may require
  // different libraries and register configurations.
  // Identify the chip before adding its driver.
  headingDegrees = -1.0f;
  return false;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED initialization failed.");
    while (true) delay(1000);
  }

  showStatus("Booting");

  if (!connectWiFi()) {
    Serial.println("Check WiFi settings in secrets.h");
  }

  // Audio pins are examples only. Audio playback is not enabled
  // until the actual amplifier wiring and TTS adapter are verified.
  audioOut = new AudioOutputI2S();
  audioOut->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audioOut->SetGain(0.5);

  updateCompassHeading();
  showStatus("Ready", "Compass/TTS need setup");
}

// ---------------- LOOP ----------------
void loop() {
  // For this starter project, request a response every 30 seconds.
  static unsigned long lastRequest = 0;

  if (millis() - lastRequest >= 30000) {
    lastRequest = millis();

    updateCompassHeading();
    showStatus("Asking Gemini");

    lastReply = askGemini(headingDegrees);
    Serial.println("Gemini: " + lastReply);

    showStatus("Response received", lastReply);
    speakText(lastReply);
  }

  // Audio playback loop is enabled once a valid WAV source
  // and provider-specific TTS adapter have been implemented.
  delay(20);
}

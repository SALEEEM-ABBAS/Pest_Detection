
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_sleep.h"

// ===== SETTINGS =====
#define WIFI_SSID "DUCHEIN"
#define WIFI_PASS "10203040"

#define FIREBASE_HOST "pest-detection-fec34-default-rtdb.asia-southeast1.firebasedatabase.app"
#define BACKEND_URL "http://10.97.191.250:5000/upload"

#define SLEEP_SECONDS 300
#define DEFAULT_FREQ 2

// ===== WIFI =====
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  Serial.println("WiFi Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 20000) {
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return false;
  }

  Serial.println("WiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

// ===== FIREBASE FREQUENCY =====
void fetchFrequency() {
  WiFiClientSecure client;
  client.setInsecure(); // Testing only

  HTTPClient http;
  http.setTimeout(8000);

  String url = "https://" + String(FIREBASE_HOST) +
               "/frequency.json";

  int freq = DEFAULT_FREQ;

  if (http.begin(client, url)) {
    int code = http.GET();

    Serial.printf("Firebase HTTP: %d\n", code);

    if (code == 200) {
      int value = http.getString().toInt();

      if (value > 0 && value <= 86400) {
        freq = value;
      }
    }

    http.end();
  }

  Serial.printf("Firebase Frequency: %d/day\n", freq);
}

// ===== CAMERA INITIALIZATION =====
bool initCamera() {
  camera_config_t c = {};

  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;

  // AI Thinker ESP32-CAM pins
  c.pin_d0 = 5;
  c.pin_d1 = 18;
  c.pin_d2 = 19;
  c.pin_d3 = 21;
  c.pin_d4 = 36;
  c.pin_d5 = 39;
  c.pin_d6 = 34;
  c.pin_d7 = 35;

  c.pin_xclk = 0;
  c.pin_pclk = 22;
  c.pin_vsync = 25;
  c.pin_href = 23;

  c.pin_sscb_sda = 26;
  c.pin_sscb_scl = 27;

  c.pin_pwdn = 32;
  c.pin_reset = -1;

  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;

  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 12;
  c.fb_count = 1;

  esp_err_t err = esp_camera_init(&c);

  if (err != ESP_OK) {
    Serial.printf("Camera Error: 0x%x\n", err);
    return false;
  }

  Serial.println("Camera Ready!");
  return true;
}

// ===== IMAGE CAPTURE & UPLOAD =====
void captureAndUpload() {
  Serial.println("Capturing Image...");

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Capture Failed!");
    return;
  }

  Serial.printf("Image Captured: %u bytes\n",
                (unsigned)fb->len);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected!");
    esp_camera_fb_return(fb);
    return;
  }

  WiFiClient client;
  HTTPClient http;

  http.setConnectTimeout(8000);
  http.setTimeout(15000);

  if (http.begin(client, BACKEND_URL)) {
    http.addHeader("Content-Type", "image/jpeg");

    int code = http.POST(fb->buf, fb->len);

    Serial.printf("Upload HTTP: %d\n", code);

    if (code >= 200 && code < 300) {
      Serial.println("Upload Successful!");
    } else if (code < 0) {
      Serial.println(http.errorToString(code));
    } else {
      Serial.println(http.getString());
    }

    http.end();
  } else {
    Serial.println("Backend Connection Failed!");
  }

  esp_camera_fb_return(fb);
}

// ===== DEEP SLEEP =====
void goToSleep() {
  Serial.println("Deep Sleep: 5 minutes");
  Serial.flush();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(
    (uint64_t)SLEEP_SECONDS * 1000000ULL
  );

  esp_deep_sleep_start();
}

// ===== MAIN =====
void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println("\n=== ESP32-CAM PEST DETECTION ===");

  // 1. Connect WiFi
  if (!connectWiFi()) {
    goToSleep();
    return;
  }

  // 2. Read Firebase frequency
  fetchFrequency();

  // 3. Initialize camera
  Serial.println("Initializing Camera...");

  if (!initCamera()) {
    goToSleep();
    return;
  }

  // 4. Capture and upload image
  captureAndUpload();

  // 5. Deep sleep
  goToSleep();
}

void loop() {}

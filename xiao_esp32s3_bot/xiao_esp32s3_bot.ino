#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "../camera.h"
#include "esp_timer.h"

// Replace these values accordingly
#define WIFI_SSID "****"
#define WIFI_PASS "****"
#define BOT_TOKEN "********"
#define CHAT_ID "*********"
// #define FB_WITH_LOCATION  // enable location in FB_msg

#include <FastBot.h>
FastBot bot(BOT_TOKEN);

const int SD_PIN_CS = 21;

fs::File videoFile;

int imageCount = 0;
bool can_capture = 1;
int8_t sd_status = 0;

bool record_video(unsigned long duration) {
  imageCount++;
  unsigned long startTime = millis();
  char filename[32];
  sprintf(filename, "/video_%d.avi", imageCount);
  videoFile = SD.open(filename, "w");
  Serial.println(filename);
  if (!videoFile) {
    Serial.println("Error opening video file!");
    return 0;
  }
  Serial.printf("Recording video\n");

  while ((millis() - startTime) < duration) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      videoFile.write(fb->buf, fb->len);
      esp_camera_fb_return(fb);
    } else {
      return 0;
    }
  }

  // close video
  videoFile.close();
  Serial.printf("Video saved\n");
  return 1;
}

void setup() {
  // initialize the Serial
  connectWiFi();

  // initialize sd card
  sd_status = connectSD();

  // set the pin connected to the LED to act as output pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // turn off the led (inverted logic!)

  bot.attach(new_msg);
}

void new_msg(FB_msg &msg) {

  if (msg.text.equalsIgnoreCase("id")) {
    bot.sendMessage(msg.chatID, msg.chatID);
  }

  // photo command
  if (msg.text.equals("/photo") && msg.chatID.equals(CHAT_ID)) {
    if (can_capture) {
      can_capture = 0;
      if (init_camera()) {
        delay(500);  // wait for camera to stabilize
        digitalWrite(LED_BUILTIN, LOW);

        // take photo
        camera_fb_t *frame = esp_camera_fb_get();

        if (frame) {
          bot.sendFile((byte *)frame->buf, frame->len, FB_PHOTO, "photo.jpg", msg.chatID);
          esp_camera_fb_return(frame);
        } else {
          bot.sendMessage("Unable to capture", msg.chatID);
        }
        digitalWrite(LED_BUILTIN, HIGH);

        deinit_camera();
      }
      can_capture = 1;
    } else {
      bot.sendMessage("Camera alreay in use", msg.chatID);
    }
  }

  // video command
  if (msg.text.equals("/video")) {
    if (sd_status <= 0) {
      bot.sendMessage(F("No SD card or SD card initialization failed!"), msg.chatID);
    } else if (can_capture) {
      can_capture = 0;
      if (init_camera()) {
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        bot.sendMessage(F("Recording..."), msg.chatID);
        bool status = record_video(15000);
        if (status) {
          delay(500);
          digitalWrite(LED_BUILTIN, HIGH);

          char filename[32];
          sprintf(filename, "/video_%d.avi", imageCount);
          Serial.println(filename);
          videoFile = SD.open(filename);
          sprintf(filename, "Video saved at location: %s", filename);
          bot.sendMessage(filename, msg.chatID);
          if (videoFile) {
            bot.sendMessage("Please wait for some time", msg.chatID);
            bot.sendFile(videoFile, FB_VIDEO, "video.avi", msg.chatID);
            videoFile.close();
          } else {
            Serial.println("Unable to open file");
            bot.sendMessage("Unable to open file", msg.chatID);
          }
        } else {
          imageCount--;
          Serial.println("Unable to recort video");
          bot.sendMessage("Unable to record video", msg.chatID);
        }
        deinit_camera();
      }
      can_capture = 1;
    } else {
      bot.sendMessage("Camera already in use", msg.chatID);
    }
  }
}

void loop() {
  bot.tick();
}

int8_t connectSD() {
  if (!SD.begin(SD_PIN_CS)) {
    Serial.println("SD card initialization failed!");
    return -1;
  }

  uint8_t cardType = SD.cardType();

  // Determine if the type of SD card is available
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return 0;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  return 1;
}

void connectWiFi() {
  delay(2000);

  Serial.begin(115200);
  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (millis() > 15000) ESP.restart();
  }
  bot.sendMessage("Connected to WiFi!", CHAT_ID);
}
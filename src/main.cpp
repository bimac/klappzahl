#include <AccelStepper.h>
#include <Audio.h>
#include <WiFiManager.h>
#include "time.h"

// pin definitions
#define MOTOR1   22 // blue
#define MOTOR2   21 // pink
#define MOTOR3   17 // yellow
#define MOTOR4   16 // orange
#define I2S_SD   26
#define I2S_GAIN 18
#define I2S_DOUT 19
#define I2S_BCLK 23
#define I2S_LRC   5

// configuration parameters
#define DEVICE_NAME "Elac RD100"
#define GAIN        HIGH                                   // HIGH=6dB, LOW=12dB
#define VOLUME      16                                     // 0...21
#define STATION     "http://www.byte.fm/stream/bytefm.m3u" // web-radio station
#define SPEED       800 // maximum motor speed [half-steps / s]
#define ACCEL       800 // motor acceleration  [half-steps / s^2]
#define NTP_SERVER  "192.168.1.1"

// global variables
namespace {
  struct tm timeinfo;
  AccelStepper stepper(AccelStepper::HALF4WIRE, MOTOR1, MOTOR3, MOTOR2, MOTOR4);
  const int stepsPerRevolution = 4096;
  TaskHandle_t Core0TaskHnd, Core1TaskHnd;
  hw_timer_t *timer = NULL;
  Audio audio;
} // namespace

void IRAM_ATTR onTimer() { stepper.move(stepsPerRevolution / 60); }

void CoreTask0(void *parameter) {
  for (;;) {
    stepper.run();
    yield();
  }
}

void CoreTask1(void *parameter) {
  for (;;) {
    audio.loop();
    yield();
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time.");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setup() {
  Serial.begin(9600);

  // initialization of wifi manager
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(10);
  wm.setHostname(DEVICE_NAME);
  wm.setConnectRetries(2);
  wm.autoConnect(DEVICE_NAME);

  // initialization of NTP time
  configTime(1, 3600, NTP_SERVER);
  printLocalTime();

  // initialization of I2S audio
  pinMode(I2S_SD, OUTPUT);
  pinMode(I2S_GAIN, OUTPUT);
  digitalWrite(I2S_SD, HIGH);
  digitalWrite(I2S_GAIN, GAIN);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(VOLUME);
  audio.forceMono(true);
  audio.connecttohost(STATION);

  // initialization of stepper motor
  stepper.setMaxSpeed(SPEED);
  stepper.setAcceleration(ACCEL);

  // initialization of timer for advancing the stepper motor
  timer = timerBegin(1, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000000, true);
  timerAlarmEnable(timer);

  // pin audio & motor tasks to different cores
  xTaskCreatePinnedToCore(CoreTask0, "motor", 1000, NULL, 0, &Core0TaskHnd, 0);
  xTaskCreatePinnedToCore(CoreTask1, "audio", 1000, NULL, 1, &Core1TaskHnd, 0);
}

void loop() {}

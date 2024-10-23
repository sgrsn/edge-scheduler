#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <stdio.h>
#include <SPI.h>
#include <WiFiUdp.h>
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>

Preferences preferences;

/* WiFi */
const char *ssid = "ESP32ap";
const char *password = "12345678";
const IPAddress ip(192, 168, 10, 1);
const IPAddress subnet(255, 255, 255, 0); 
WiFiUDP udp;
WebServer server(80);

/* ESC */
Servo esc;
const int esc_pin = D0;
int esc_minUs = 800;
int esc_maxUs = 2000;

/* Servo for Horizontal Wing */
Servo servo;
const int servo_pin = D2;
int servo_minUs = 1000;
int servo_maxUs = 2000;
int servo_neutral = 1500;

/* Servo for Vertical Wing */
Servo servo2;
const int servo2_pin = D1;
int servo2_minUs = 1200;
int servo2_maxUs = 1800;
int servo2_neutral = 1500;

const unsigned long WATCHDOG_TIMEOUT = 1000;

class Schedule
{
public:
  Schedule(){}
  void registerSchedule(std::vector<std::pair<double, double>> schedule) 
  {
    schedule_ = schedule;
    index_ = 0;
    isRunning_ = false;
  }
  void start()
  {
    isRunning_ = true;
    lastTime_ = (double) millis() / 1000.0;
  }
  bool isRunning()
  {
    return isRunning_;
  }
  double getOutput()
  {
    if (!isRunning_) {
      return 0;
    }

    double currentTime = (double)millis() / 1000.0 - lastTime_;
    
    double x_0 = schedule_[index_].first;
    double y_0 = schedule_[index_].second;
    double x_1 = schedule_[index_ + 1].first;
    double y_1 = schedule_[index_ + 1].second;

    double y = (y_1 - y_0) / (x_1 - x_0) * (currentTime - x_0) + y_0;

    if (currentTime > x_1) {
      index_++;
    }

    if (index_ >= schedule_.size() - 1) {
      index_ = 0;
      isRunning_ = false;
    }

    return y;
  }
private:
  std::vector<std::pair<double, double>> schedule_;
  int index_;
  bool isRunning_;
  double lastTime_;
};

class Watchdog
{
public:
  Watchdog(unsigned long timeout_ms) : timeout_ms_(timeout_ms)
  {
    lastTime_ = millis();
  }
  void reset()
  {
    lastTime_ = millis();
  }
  bool isTimeout()
  {
    unsigned long currentTime = millis() - lastTime_;
    return currentTime > timeout_ms_;
  }
private:
  unsigned long timeout_ms_;
  unsigned long lastTime_;
};

Schedule schedule;
Watchdog watchdog(WATCHDOG_TIMEOUT);

void handleGetSchedule() 
{
  // 受信するデータ
  std::vector<std::pair<double, double>> data;
  
  // 受信
  Serial.println("handleGetSchedule");
  String json = server.arg("points");
  // パース
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, json);
  data.clear();
  for (int i = 0; i < doc.size(); i++) {
    double x = doc[i]["x"];
    double y = doc[i]["y"];
    data.push_back(std::make_pair(x, y));
  } 
  schedule.registerSchedule(data);
  schedule.start();
}

void handleGetJoystick() 
{
  // 受信するデータ
  double x = 0;
  double y = 0;

  // 受信
  //Serial.println("handleGetJoystick");
  String json = server.arg("joystick");
  // パース
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, json);
  // 送信側
  x = doc["x"];
  y = doc["y"];

  x = constrain(-x*100, -100.0, 100.0);
  y = constrain(-y*100, -100.0, 100.0);

  /* Control horizontal servo */
  int servo_min_duty = map(servo_minUs, 0, 20000, 0, 1023);
  int servo_max_duty = map(servo_maxUs, 0, 20000, 0, 1023);
  int duty1 = map(y, -100, 100, servo_min_duty, servo_max_duty);
  ledcWrite(2, duty1);

  int servo2_min_duty = map(servo2_minUs, 0, 20000, 0, 1023);
  int servo2_max_duty = map(servo2_maxUs, 0, 20000, 0, 1023);
  int duty2 = map(x, -100, 100, servo2_min_duty, servo2_max_duty);
  ledcWrite(3, duty2);
}

// クライアントから一定時間ごとに送信されるリクエストを受け取る
// watchdogをリセットする
void handleWatchdog() 
{
  // timestamp_msを取得
  double timestamp_ms = server.arg("timestamp_ms").toDouble();  // maybe unused

  // watchdogをリセット
  watchdog.reset();
}

/*Timer*/
void onTimer() {
  double output = schedule.getOutput();
  int esc_min_duty = map(esc_minUs, 0, 20000, 0, 1023);
  int esc_max_duty = map(esc_maxUs, 0, 20000, 0, 1023);
  ledcWrite(1, map(constrain(output, 0, 100), 0, 100, esc_min_duty, esc_max_duty));
  //Serial.printf("output: %f\n", output);

  // watchdogがタイムアウトしたらESCを最小にする
  if (watchdog.isTimeout()) {
    ledcWrite(1, esc_min_duty);
    ledcWrite(2, map(servo_neutral, 0, 20000, 0, 1023));
    ledcWrite(3, map(servo2_neutral, 0, 20000, 0, 1023));
  }
}


void setup() {
  /* Serial */
  Serial.begin(9600);

  /* Calibration ESC */
  pinMode(esc_pin, OUTPUT);
  ledcSetup(1, 50, 10);
  ledcAttachPin(esc_pin, 1);
  int esc_min_duty = map(esc_minUs, 0, 20000, 0, 1023);
  ledcWrite(1, esc_min_duty);

  /* Calibration Horizontal Servo */
  pinMode(servo_pin, OUTPUT);
  ledcSetup(2, 50, 10);
  ledcAttachPin(servo_pin, 2);
  int servo_min_duty = map(servo_minUs, 0, 20000, 0, 1023);
  ledcWrite(2, servo_min_duty);

  /* Calibration Vertical Servo */
  pinMode(servo2_pin, OUTPUT);
  ledcSetup(3, 50, 10);
  ledcAttachPin(servo2_pin, 3);
  int servo2_min_duty = map(servo2_neutral, 0, 20000, 0, 1023);
  ledcWrite(3, servo2_min_duty);

  /* Wifi access point Setup */
  Serial.println();
  Serial.print("Configuring access point...");
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(ip, ip, subnet);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  /* Timer (100ms)*/
  Serial.println("begin timer");
  auto timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 100000, true);
  timerAlarmEnable(timer);

  /* UDP */
  unsigned int localUdpPort = 4210;
  Serial.println("begin UDP port");
  udp.begin(localUdpPort);
  Serial.print("local UDP port: ");
  Serial.println(localUdpPort);
  
  /* SPIFFS */
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS failed to mount");
    Serial.println("Please run following command to format SPIFFS:");
    Serial.println("\033[31mpio run -t uploadfs\033[0m");
  } else {
    Serial.println("SPIFFS OK");
  }

  /* WebServer */
  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/styles.css", SPIFFS, "/styles.css");
  server.serveStatic("/joystick.html", SPIFFS, "/joystick.html");
  server.serveStatic("/joystick.js", SPIFFS, "/joystick.js");
  server.serveStatic("/scheduler.html", SPIFFS, "/scheduler.html");
  server.serveStatic("/scheduler.js", SPIFFS, "/scheduler.js");
  server.serveStatic("/watchdog.js", SPIFFS, "/watchdog.js");

  server.on("/get-schedule", HTTP_PUT, handleGetSchedule);
  server.on("/get-joystick", HTTP_PUT, handleGetJoystick);
  server.on("/watchdog", HTTP_PUT, handleWatchdog);
  server.begin();
  Serial.println("start server");
}

void loop() {
  server.handleClient();
}
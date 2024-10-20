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

Schedule schedule;

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

/*Timer*/
void onTimer() {
  double output = schedule.getOutput();
  int esc_min_duty = map(esc_minUs, 0, 20000, 0, 1023);
  int esc_max_duty = map(esc_maxUs, 0, 20000, 0, 1023);
  ledcWrite(1, map(constrain(output, 0, 100), 0, 100, esc_min_duty, esc_max_duty));
  Serial.printf("output: %f\n", output);
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

  /* Wifi access point Setup */
  Serial.println();
  Serial.print("Configuring access point...");
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(ip, ip, subnet);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  /* Timer (50ms)*/
  Serial.println("begin timer");
  auto timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 50000, true);
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
  server.serveStatic("/", SPIFFS, "/scheduler.html");
  server.on("/get-schedule", HTTP_PUT, handleGetSchedule);
  server.begin();
  Serial.println("start server");
}

void loop() {
  server.handleClient();
}
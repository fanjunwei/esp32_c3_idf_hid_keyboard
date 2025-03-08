#include <Adafruit_AHTX0.h>
#include <Arduino.h>
// #include <NTPClient.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <stdio.h>

#include "chinese_14.h"

#define FONT_SIZE 14
// 巴法云服务器地址默认即可
#define TCP_SERVER_ADDR "bemfa.com"

///****************需要修改的地方*****************///

// 服务器端口//TCP创客云端口8344//TCP设备云端口8340
#define TCP_SERVER_PORT "8344"
// WIFI名称，区分大小写，不要写错
#define DEFAULT_STASSID "fan"
// WIFI密码
#define DEFAULT_STAPSW "fanfan123456"
// 用户私钥，可在控制台获取,修改为自己的UID
#define UID "29cfad8d56fe41c1910c4c3fa4d20d04"
// 主题名字，可在控制台新建
#define TOPIC "af004"
// 单片机LED引脚值
// const int LED_Pin = 12;
int led_state = 0;
bool aht_init = false;
sensors_event_t humidity, temp;

///*********************************************///
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

// led 控制函数
void turnOnLed();
void turnOffLed();

// 最大字节数
#define MAX_PACKETSIZE 512
// 设置心跳值30s
#define KEEPALIVEATIME 30 * 1000
#define LED_IVEATIME 1 * 1000
#define SEND_IVEATIME 10 * 1000
#define BUTTON_R1 6
#define BUTTON_R2 8
#define BUTTON_R3 2

#define BUTTON_C1 12
#define BUTTON_C2 13
#define BUTTON_C3 10

#define DEBOUNCE_DELAY 10
bool button_clicked = false;
const uint8_t button_r[3] = {BUTTON_R1, BUTTON_R2, BUTTON_R3};
const uint8_t button_c[3] = {BUTTON_C1, BUTTON_C2, BUTTON_C3};
bool button_state[3][3] = {0};

// tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
String TcpClient_Buff = "";
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;     // 心跳
unsigned long ledTick = 0;          // 屏幕刷新
unsigned long sendTick = 0;         // 发送时间
unsigned long preTCPStartTick = 0;  // 连接
bool preTCPConnected = false;

// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP);
// NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 8 * 3600, 60000);
uint8_t sdaPin = 4;  // JTAG TDI
uint8_t sclPin = 5;  // JTAG TCK
// SoftWire SOFT_I2C(sdaPin, sclPin);
Adafruit_AHTX0 aht;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/sclPin,
    /* data=*/sdaPin);  // ESP32 Thing, HW I2C with pin remapping
// 相关函数初始化
// 连接WIFI
void doWiFiTick();
void startSTA();

// TCP初始化连接
void doTCPClientTick();
void startTCPClient();
void sendtoTCPServer(String p);
void IRAM_ATTR ButtonISR();

/*
 *发送数据到TCP服务器
 */
void sendtoTCPServer(String p) {
  if (!TCPclient.connected()) {
    Serial.println("Client is not readly");
    return;
  }
  TCPclient.print(p);
  Serial.println("[Send to TCPServer]:String");
  Serial.println(p);
}

/*
 *初始化和服务器建立连接
 */
void startTCPClient() {
  if (TCPclient.connect(TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT))) {
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n", TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT));
    char tcpTemp[128];
    sprintf(tcpTemp, "cmd=1&uid=%s&topic=%s\r\n", UID, TOPIC);

    sendtoTCPServer(tcpTemp);
    preTCPConnected = true;
    preHeartTick = millis();
    TCPclient.setNoDelay(true);
  } else {
    Serial.print("Failed connected to server:");
    Serial.println(TCP_SERVER_ADDR);
    TCPclient.stop();
    preTCPConnected = false;
  }
  preTCPStartTick = millis();
}

/*
 *检查数据，发送心跳
 */
void doTCPClientTick() {
  // 检查是否断开，断开后重连
  if (WiFi.status() != WL_CONNECTED) return;

  if (!TCPclient.connected()) {  // 断开重连

    if (preTCPConnected == true) {
      preTCPConnected = false;
      preTCPStartTick = millis();
      Serial.println();
      Serial.println("TCP Client disconnected.");
      TCPclient.stop();
    } else if (millis() - preTCPStartTick > 1 * 1000)  // 重新连接
      startTCPClient();
  } else {
    // timeClient.update();
    if (TCPclient.available()) {  // 收数据
      char c = TCPclient.read();
      TcpClient_Buff += c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();

      if (TcpClient_BuffIndex >= MAX_PACKETSIZE - 1) {
        TcpClient_BuffIndex = MAX_PACKETSIZE - 2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
      preHeartTick = millis();
    }
    if (millis() - preHeartTick >= KEEPALIVEATIME) {  // 保持心跳
      preHeartTick = millis();
      Serial.println("--Keep alive:");
      sendtoTCPServer("cmd=0&msg=keep\r\n");
    }
    if (millis() - sendTick >= SEND_IVEATIME) {
      char buf[128] = {0};
      sendTick = millis();
      sprintf(buf, "cmd=2&uid=%s&topic=%s&msg=#%0.2f#%0.2f#%s\r\n", UID, TOPIC,
              temp.temperature, humidity.relative_humidity,
              led_state ? "on" : "off");
      sendtoTCPServer(buf);
    }
  }
  if ((TcpClient_Buff.length() >= 1) &&
      (millis() - TcpClient_preTick >= 200)) {  // data ready
    TCPclient.flush();
    Serial.println("Buff");
    Serial.println(TcpClient_Buff);
    if ((TcpClient_Buff.indexOf("&msg=on") > 0)) {
      turnOnLed();
    } else if ((TcpClient_Buff.indexOf("&msg=off") > 0)) {
      turnOffLed();
    }
    TcpClient_Buff = "";
    TcpClient_BuffIndex = 0;
  }
}

void startSTA() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_STASSID, DEFAULT_STAPSW);
}

void initAHT() {
  if (!aht_init) {
    printf("init aht\n");
    if (aht.begin()) {
      aht_init = true;
    } else {
      printf("Could not find AHT? Check wiring \n");
    }
  }
}
/**************************************************************************
                                 WIFI
***************************************************************************/
/*
  WiFiTick
  检查是否需要初始化WiFi
  检查WiFi是否连接上，若连接成功启动TCP Client
  控制指示灯
*/
void doWiFiTick() {
  static bool startSTAFlag = false;
  static bool taskStarted = false;
  static uint32_t lastWiFiCheckTick = 0;

  if (!startSTAFlag) {
    startSTAFlag = true;
    startSTA();
    Serial.printf("Heap size:%d\r\n", ESP.getFreeHeap());
  }

  // 未连接1s重连
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiCheckTick > 1000) {
      lastWiFiCheckTick = millis();
    }
  }
  // 连接成功建立
  else {
    if (taskStarted == false) {
      taskStarted = true;
      Serial.print("\r\nGet IP Address: ");
      Serial.println(WiFi.localIP());
      startTCPClient();
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }
  }
}
// 打开灯泡
void turnOnLed() {
  Serial.println("Turn ON");
  led_state = 1;
  // digitalWrite(LED_Pin, LOW);
}
// 关闭灯泡
void turnOffLed() {
  Serial.println("Turn OFF");
  led_state = 0;
  // digitalWrite(LED_Pin, HIGH);
}
void showled() {
  if (millis() - ledTick >= LED_IVEATIME) {
    initAHT();
    ledTick = millis();
    char buf[30] = {0};
    u8g2.clearBuffer();
    int y = FONT_SIZE;
    u8g2.setFont(chinese_14);
    if (aht_init) {
      // populate temp and humidity objects with fresh data
      aht.getEvent(&humidity, &temp);
      // printf("Temperature: %0.2f C \n", temp.temperature);
      // printf("Humidity: %0.2f %", humidity.relative_humidity);
      sprintf(buf, "温度: %0.2f ℃", temp.temperature);
      u8g2.drawUTF8(0, y, buf);
      y += FONT_SIZE;
      sprintf(buf, "湿度: %0.2f %%", humidity.relative_humidity);
      u8g2.drawUTF8(0, y, buf);
      y += FONT_SIZE;
    }
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char str[30] = {0};
      snprintf(str, 30, "%04d-%02d-%02d", timeinfo.tm_year + 1900,
               timeinfo.tm_mon + 1, timeinfo.tm_mday);
      u8g2.drawUTF8(0, y, str);
      y += FONT_SIZE;
      snprintf(str, 30, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min,
               timeinfo.tm_sec);
      u8g2.drawUTF8(0, y, str);
      y += FONT_SIZE;
    }
    u8g2.sendBuffer();
  }
}
void initInterrupt() {
  for (uint8_t j = 0; j < 3; j++) {
    attachInterrupt(button_c[j], ButtonISR, FALLING);
  }
}
void closeInterrupt() {
  for (uint8_t j = 0; j < 3; j++) {
    detachInterrupt(button_c[j]);
  }
}
void buttonWaitInterrupt() {
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(button_r[i], OUTPUT);
    digitalWrite(button_r[i], LOW);
  }
  for (uint8_t j = 0; j < 3; j++) {
    pinMode(button_c[j], INPUT_PULLUP);
  }
}
void scanButtonInit() {
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(button_r[i], OUTPUT);
    digitalWrite(button_r[i], HIGH);
  }
  for (uint8_t j = 0; j < 3; j++) {
    pinMode(button_c[j], INPUT_PULLUP);
  }
}
bool isButtonDown(uint8_t row, uint8_t col, bool only_read) {
  digitalWrite(button_r[row], LOW);
  // if (!only_read) {
  //   for (uint8_t i = 0; i < 3; i++) {
  //     if (row != i) {
  //       pinMode(button_r[i], INPUT_PULLUP);
  //     } else {
  //       pinMode(button_r[i], OUTPUT);
  //       digitalWrite(button_r[i], LOW);
  //     }
  //   }
  //   for (uint8_t j = 0; j < 3; j++) {
  //     pinMode(button_c[j], INPUT_PULLUP);
  //   }
  // }
  bool res = !digitalRead(button_c[col]);
  digitalWrite(button_r[row], HIGH);
  return res;
}

void scan_button() {
  for (uint8_t i = 0; i < 3; i++) {
    for (uint8_t j = 0; j < 3; j++) {
      if (isButtonDown(i, j, false)) {
        if (!button_state[i][j]) {
          delay(DEBOUNCE_DELAY);
          if (isButtonDown(i, j, true)) {
            button_state[i][j] = true;
            printf("button [%d,%d] down \n", i, j);
          }
        }
      } else {
        if (button_state[i][j]) {
          delay(DEBOUNCE_DELAY);
          if (!isButtonDown(i, j, true)) {
            button_state[i][j] = false;
            printf("button [%d,%d] up \n", i, j);
          }
        }
      }
    }
  }
}

void setup() {
  char buf[] = "init ...";
  Serial.begin(115200);
  printf("setup  .......   \n");
  // pinMode(LED_Pin, OUTPUT);
  // digitalWrite(LED_Pin, HIGH);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g_font_10x20);
  u8g2.drawUTF8(0, 20, buf);
  u8g2.sendBuffer();
  Wire.begin(sdaPin, sclPin);
  // buttonWaitInterrupt();
  // initInterrupt();
  scanButtonInit();
}

/*
按键矩阵
6  R1
8  R2
11 R3 --> 2 ?

12 C1 --> 7
13 C2 --> 3
18 C3 --> 10
*/
void loop() {
  doWiFiTick();
  doTCPClientTick();
  showled();
  // if (button_clicked) {
  //   closeInterrupt();
  //   scanButtonInit();
  //   scan_button();
  //   buttonWaitInterrupt();
  //   initInterrupt();
  // }
  scan_button();
  // pinMode(11, OUTPUT);
  // digitalWrite(11, LOW);
  // char buf[30] = {0};
  // u8g2.clearBuffer();
  // u8g2.setFont(u8g2_font_10x20_me);
  // pinMode(6, INPUT_PULLUP);
  // pinMode(11, INPUT_PULLUP);
  // pinMode(8, OUTPUT);
  // digitalWrite(8, LOW);

  // int state;
  // pinMode(2, INPUT_PULLUP);
  // state = digitalRead(2);
  // printf("io2 : %d\n", state);
  // delay(500);
  // pinMode(10, INPUT_PULLUP);
  // state = digitalRead(10);
  // printf("io10 : %d\n", state);
  // pinMode(3, INPUT_PULLUP);
  // state = digitalRead(3);
  // printf("io03 : %d\n", state);
}
void IRAM_ATTR ButtonISR() { button_clicked = true; }
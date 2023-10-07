#include <ArduinoJson.h>//JSON解析
#include <ESP8266WiFi.h>//WIFI
#include <SPI.h>
#include <U8g2lib.h>
#include <WiFiUdp.h>
#include <TimeLib.h>//时间
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include <AsyncDelay.h>
#include "mario_cannon.h"

const int OLED_WIDTH = 128;
const int OLED_HEIGHT = 32;
const int FPS = 1000 / 30; //30FPS
const int X = 1;
int pos_x = 0;
int pos_y = 0;

/*
U8G2_GP1287AI_256X50_1_4W_SW_SPI(rotation, clock, data, cs, dc [, reset]) [page buffer, size = 256 bytes]
U8G2_GP1287AI_256X50_2_4W_SW_SPI(rotation, clock, data, cs, dc [, reset]) [page buffer, size = 512 bytes]
U8G2_GP1287AI_256X50_F_4W_SW_SPI(rotation, clock, data, cs, dc [, reset]) [full framebuffer, size = 1792 bytes]
U8G2_GP1287AI_256X50_1_4W_HW_SPI(rotation, cs, dc [, reset]) [page buffer, size = 256 bytes]
U8G2_GP1287AI_256X50_2_4W_HW_SPI(rotation, cs, dc [, reset]) [page buffer, size = 512 bytes]
U8G2_GP1287AI_256X50_1_2ND_4W_HW_SPI(rotation, cs, dc [, reset]) [page buffer, size = 256 bytes]
U8G2_GP1287AI_256X50_2_2ND_4W_HW_SPI(rotation, cs, dc [, reset]) [page buffer, size = 512 bytes]
U8G2_GP1287AI_256X50_F_2ND_4W_HW_SPI(rotation, cs, dc [, reset]) [full framebuffer, size = 1792 bytes]
*/

//WEMOS D1
//U8G2_GP1287AI_256X50_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ D13, /* data=*/ D11, /* cs=*/ D10, /* dc=*/ U8X8_PIN_NONE, /* reset=*/ D9);  

//WEMOS D1 MINI
//U8G2_GP1287AI_256X50_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ D5, /* data=*/ D7, /* cs=*/ D8, /* dc=*/ U8X8_PIN_NONE, /* reset=*/ D1); 
U8G2_GP1287AI_256X50_F_4W_HW_SPI u8g2(U8G2_R0,/* cs=*/ D8, /* dc=*/ U8X8_PIN_NONE ,/* reset=*/ D1);

//U8G2_GP1287AI_256X50_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 2, /* data=*/ 4, /* cs=*/ 12, /* dc=*/ U8X8_PIN_NONE, /* reset=*/ 16); 


WiFiUDP Udp;
unsigned int localPort = 8888; // 用于侦听UDP数据包的本地端口

//网络校时的相关配置
static const char ntpServerName[] = "ntp1.aliyun.com"; //NTP服务器，使用阿里云
int timeZone = 8; //时区设置，采用东8区

//保存断网前的最新数据
int results_0_now_temperature_int_old=-99;
String results_0_now_text_str_old;
int results_0_now_code_int_old=34;
int results_0_daily_1_high_int_old;
int results_0_daily_1_low_int_old;
String results_0_daily_1_text_day_str_old;

int results_0_daily_0_rainfall_int_old=99;
String results_0_daily_0_wind_direction_str_old="SE";
int results_0_daily_0_wind_scale_int_old=9;
String results_0_daily_0_humidity_str_old="99";
int results_0_daily_0_high_int_old ;
int results_0_daily_0_low_int_old; 

//函数声明
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
void oledClockDisplay();
void sendCommand(int command, int value);
void initdisplay();
void connectWiFi();
void parseInfo_now(WiFiClient client,int i);
void parseInfo_fut(WiFiClient client,int i);
//
boolean isNTPConnected = false;

/*const unsigned char xing[] U8X8_PROGMEM = {
    0x00, 0x00, 0xF8, 0x0F, 0x08, 0x08, 0xF8, 0x0F, 0x08, 0x08, 0xF8, 0x0F, 0x80, 0x00, 0x88, 0x00,
    0xF8, 0x1F, 0x84, 0x00, 0x82, 0x00, 0xF8, 0x0F, 0x80, 0x00, 0x80, 0x00, 0xFE, 0x3F, 0x00, 0x00};
const unsigned char liu[] U8X8_PROGMEM = {
    0x40, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x02, 0x20, 0x04, 0x10, 0x08, 0x10, 0x10, 0x08, 0x10, 0x04, 0x20, 0x02, 0x20, 0x00, 0x00}; 
*/






typedef struct
{                  //存储配置结构体
    int tz;        //时间戳
} config_type;
config_type config;

WiFiClient clientNULL;
DNSServer dnsServer;
ESP8266WebServer server(80);


//----------WIFI连接配置----------
//const char* ssid     = "Tree New Bee";       // 连接WiFi名（此处使用XXX为示例）                                   
//const char* password = "13815891411";          // 连接WiFi密码（此处使用12345678为示例）
const char* ssid     = "inchi";       // 连接WiFi名（此处使用XXX为示例）                                   
const char* password = "yq071010";          // 连接WiFi密码（此处使用12345678为示例）
                                            // 请将您需要连接的WiFi密码填入引号中
//----------天气API配置----------
const char* host = "api.seniverse.com";   // 将要连接的服务器地址  
const int httpPort = 80;              // 将要连接的服务器端口      

// 心知天气HTTP请求所需信息
String reqUserKey = "SwDdpjO7d22zOstkD";   // 私钥
String reqLocation = "WTSQQYHVQ973";            // 城市WTSTB2RFJXXJ
String reqUnit = "c";                      // 摄氏/华氏

//----------设置屏幕----------
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
int sta = 0;
//----------初始化OLED----------
void initdisplay()
{
    u8g2.begin();
    u8g2.enableUTF8Print();
}
//----------用于获取实时天气的函数(0)----------
void TandW(){
  String reqRes = "/v3/weather/now.json?key=" + reqUserKey +
                  + "&location=" + reqLocation + 
                  "&language=en&unit=" +reqUnit;
  // 向心知天气服务器服务器请求信息并对信息进行解析
  httpRequest(reqRes,0);
  //延迟，需要低于20次/分钟
  delay(3000);
}
void display_1(int results_0_now_temperature_int,String results_0_now_text_str);//声明函数，用于显示温度、天气


//----------获取3天预报(1)----------
void threeday(){
  // 建立心知天气API当前天气请求资源地址
  String reqRes = "/v3/weather/daily.json?key=" + reqUserKey +
                  + "&location=" + reqLocation + "&language=en&unit=" +
                  reqUnit + "&start=0&days=3";

  // 向心知天气服务器服务器请求信息并对信息进行解析
  httpRequest(reqRes,1);
  delay(3000);
}

void clock_display(time_t prevDisplay){
    server.handleClient();
    dnsServer.processNextRequest();
    if (timeStatus() != timeNotSet)
    {
        if (now() != prevDisplay)
        { //时间改变时更新显示
            prevDisplay = now();
            oledClockDisplay();
        }
    }
}


void setup(){
  Serial.begin(9600);          
  Serial.println("");
  initdisplay();
  // 连接WiFi

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
  u8g2.setCursor(0, 14);
  u8g2.print("Waiting for WiFi");
  u8g2.setCursor(0, 30);
  u8g2.print("connection...");
  u8g2.sendBuffer();
  connectWiFi();
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(600); //每300秒同步一次时间

  
}

time_t prevDisplay = 0; //当时钟已经显示
 
void loop(){
    if (sta>=0 && sta<=250){
      clock_display(prevDisplay);
       
    }else if(sta == 251){
      TandW();
      
    }else{
      threeday();
      
    }

    ++sta;

    if(sta==253){
      sta = 0;
    }
}
 
// 向心知天气服务器服务器请求信息并对信息进行解析
void httpRequest(String reqRes,int stat){
  WiFiClient client;

  // 建立http请求信息
  String httpRequest = String("GET ") + reqRes + " HTTP/1.1\r\n" + 
                              "Host: " + host + "\r\n" + 
                              "Connection: close\r\n\r\n";
  Serial.println(""); 
  Serial.print("Connecting to "); Serial.print(host);

  // 尝试连接服务器
  if (client.connect(host, 80)){
    Serial.println(" Success!");
 
    // 向服务器发送http请求信息
    client.print(httpRequest);
    Serial.println("Sending request: ");
    Serial.println(httpRequest);  
 
    // 获取并显示服务器响应状态行 
    String status_response = client.readStringUntil('\n');
    Serial.print("status_response: ");
    Serial.println(status_response);
 
    // 使用find跳过HTTP响应头
    if (client.find("\r\n\r\n")) {
      Serial.println("Found Header End. Start Parsing.");
    }
    if (stat == 0){
      
      // 利用ArduinoJson库解析心知天气响应信息(实时数据)
      parseInfo_now(client,1); 
    }else if(stat == 1){
      parseInfo_fut(client,1);
    }
  }
  else {
    Serial.println(" connection failed!");
    if (stat == 0){
      
      // 利用ArduinoJson库解析心知天气响应信息(实时数据)
      parseInfo_now(clientNULL,0); 
    }else if(stat == 1){
      parseInfo_fut(clientNULL,0);
    }
  }
    
  //断开客户端与服务器连接工作
  client.stop(); 
}

// 连接WiFi
void connectWiFi(){
  WiFi.begin(ssid, password);                  // 启动网络连接
  Serial.print("Connecting to ");              // 串口监视器输出网络连接信息
  Serial.print(ssid); Serial.println(" ...");  // 告知用户NodeMCU正在尝试WiFi连接
  
  int i = 0;                                   // 这一段程序语句用于检查WiFi是否连接成功
  while (WiFi.status() != WL_CONNECTED) {      // WiFi.status()函数的返回值是由NodeMCU的WiFi连接状态所决定的。 
    delay(1000);                               // 如果WiFi连接成功则返回值为WL_CONNECTED                       
    Serial.print(i++); Serial.print(' ');      // 此处通过While循环让NodeMCU每隔一秒钟检查一次WiFi.status()函数返回值
  }                                            // 同时NodeMCU将通过串口监视器输出连接时长读秒。
                                               // 这个读秒是通过变量i每隔一秒自加1来实现的。                                              
  Serial.println("");                          // WiFi连接成功后
  Serial.println("Connection established!");   // NodeMCU将通过串口监视器输出"连接成功"信息。
  Serial.print("IP address:    ");             // 同时还将输出NodeMCU的IP地址。这一功能是通过调用
  Serial.println(WiFi.localIP());              // WiFi.localIP()函数来实现的。该函数的返回值即NodeMCU的IP地址。  
}

// 利用ArduinoJson库解析心知天气响应信息(实时)
void parseInfo_now(WiFiClient client,int i){

  if(i==1){
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(6) + 230;
  DynamicJsonDocument doc(capacity);
  
  deserializeJson(doc, client);
  
  JsonObject results_0 = doc["results"][0];
  
  JsonObject results_0_now = results_0["now"];
  const char* results_0_now_text = results_0_now["text"]; // "Sunny"
  const char* results_0_now_code = results_0_now["code"]; // "0"
  const char* results_0_now_temperature = results_0_now["temperature"]; // "32"
  
  
  const char* results_0_last_update = results_0["last_update"]; // "2020-06-02T14:40:00+08:00" 
 
  // 通过串口监视器显示以上信息
  String results_0_now_text_str = results_0_now["text"].as<String>(); 
  int results_0_now_code_int = results_0_now["code"].as<int>(); 
  int results_0_now_temperature_int = results_0_now["temperature"].as<int>(); 
  String results_0_last_update_str = results_0["last_update"].as<String>();   
 
  Serial.println(F("======Weahter Now======="));
  Serial.print(F("Weather Now: "));
  Serial.print(results_0_now_text_str);
  Serial.print(F(" "));
  Serial.println(results_0_now_code_int);
  Serial.print(F("Temperature: "));
  Serial.println(results_0_now_temperature_int);
  Serial.print(F("Last Update: "));
  Serial.println(results_0_last_update_str);
  Serial.println(F("========================"));
  display_0(results_0_now_temperature_int,results_0_now_text_str,results_0_daily_1_high_int_old,results_0_daily_1_low_int_old,results_0_daily_1_text_day_str_old);
  results_0_now_text_str_old = results_0_now_text_str;
  results_0_now_code_int_old = results_0_now_code_int;
  results_0_now_temperature_int_old = results_0_now_temperature_int;
  }
  else{
    display_0(results_0_now_temperature_int_old,results_0_now_text_str_old,results_0_daily_1_high_int_old,results_0_daily_1_low_int_old,results_0_daily_1_text_day_str_old);
  }
 
}
//----------输出实时天气----------
void display_0(int results_0_now_temperature_int,String results_0_now_text_str,int results_0_daily_1_high_int,int results_0_daily_1_low_int,String results_0_daily_1_text_day_str){
  //显示输出
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.setCursor(15, 14);
  u8g2.print("南京今天天气");
 u8g2.setFont(u8g2_font_logisoso24_tr);
  u8g2.setCursor(15, 44);
  u8g2.print(results_0_daily_0_low_int_old);
  u8g2.setCursor(51, 44);
  u8g2.print("~");
  u8g2.setCursor(70, 44);
  u8g2.print(results_0_daily_0_high_int_old);

  
  u8g2.setCursor(130, 14);
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.print("南京明天天气");
  u8g2.setFont(u8g2_font_logisoso24_tr);
  u8g2.setCursor(130, 44);
  u8g2.print(results_0_daily_1_low_int);
  u8g2.setCursor(166, 44);
  u8g2.print("~");
  u8g2.setCursor(185, 44);
  u8g2.print(results_0_daily_1_high_int);
  
  u8g2.setCursor(140, 62);
  u8g2.setFont(u8g2_font_unifont_t_chinese2);
  u8g2.print(results_0_daily_1_text_day_str);
  u8g2.sendBuffer();
}
/*
void display_0(int results_0_now_temperature_int,String results_0_now_text_str){
  //显示输出
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.setCursor(15, 14);
  u8g2.print("南京实时天气");
  u8g2.setFont(u8g2_font_logisoso24_tr);
  u8g2.setCursor(45, 44);
  u8g2.print(results_0_now_temperature_int);
  u8g2.setCursor(35, 61);
  u8g2.setFont(u8g2_font_unifont_t_chinese2);
  u8g2.print(results_0_now_text_str);
  u8g2.sendBuffer();
  }
void display_1(int results_0_daily_1_high_int,int results_0_daily_1_low_int,String results_0_daily_1_text_day_str){
  //显示输出
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.setCursor(15, 14);
  u8g2.print("南京明天天气");
  
  u8g2.setFont(u8g2_font_logisoso24_tr);
  u8g2.setCursor(20, 46);
  u8g2.print(results_0_daily_1_low_int);
  u8g2.setCursor(56, 46);
  u8g2.print("~");
  u8g2.setCursor(75, 46);
  u8g2.print(results_0_daily_1_high_int);
  
  u8g2.setCursor(30, 62);
  u8g2.setFont(u8g2_font_unifont_t_chinese2);
  u8g2.print(results_0_daily_1_text_day_str);
  u8g2.sendBuffer();
}
*/
// 利用ArduinoJson库解析心知天气响应信息(预测)
void parseInfo_fut(WiFiClient client,int i){
  if(i==1){
    
  
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(6) + 3*JSON_OBJECT_SIZE(14) + 860;
  
  DynamicJsonDocument doc(capacity);
  
  deserializeJson(doc, client);
  
  JsonObject results_0 = doc["results"][0];
  
  JsonArray results_0_daily = results_0["daily"];
  
  JsonObject results_0_daily_0 = results_0_daily[0];
  const char* results_0_daily_0_date = results_0_daily_0["date"]; 
  const char* results_0_daily_0_text_day = results_0_daily_0["text_day"]; 
  const char* results_0_daily_0_code_day = results_0_daily_0["code_day"];
  const char* results_0_daily_0_text_night = results_0_daily_0["text_night"]; 
  const char* results_0_daily_0_code_night = results_0_daily_0["code_night"]; 
  const char* results_0_daily_0_high = results_0_daily_0["high"];
  const char* results_0_daily_0_low = results_0_daily_0["low"]; 
  const char* results_0_daily_0_rainfall = results_0_daily_0["rainfall"];
  const char* results_0_daily_0_precip = results_0_daily_0["precip"]; 
  const char* results_0_daily_0_wind_direction = results_0_daily_0["wind_direction"]; 
  const char* results_0_daily_0_wind_direction_degree = results_0_daily_0["wind_direction_degree"];
  const char* results_0_daily_0_wind_speed = results_0_daily_0["wind_speed"];
  const char* results_0_daily_0_wind_scale = results_0_daily_0["wind_scale"];
  const char* results_0_daily_0_humidity = results_0_daily_0["humidity"];
  
  JsonObject results_0_daily_1 = results_0_daily[1];
  const char* results_0_daily_1_date = results_0_daily_1["date"];
  const char* results_0_daily_1_text_day = results_0_daily_1["text_day"];
  const char* results_0_daily_1_code_day = results_0_daily_1["code_day"];
  const char* results_0_daily_1_text_night = results_0_daily_1["text_night"]; 
  const char* results_0_daily_1_code_night = results_0_daily_1["code_night"]; 
  const char* results_0_daily_1_high = results_0_daily_1["high"];
  const char* results_0_daily_1_low = results_0_daily_1["low"]; 
  const char* results_0_daily_1_rainfall = results_0_daily_1["rainfall"]; 
  const char* results_0_daily_1_precip = results_0_daily_1["precip"]; 
  const char* results_0_daily_1_wind_direction = results_0_daily_1["wind_direction"];
  const char* results_0_daily_1_wind_direction_degree = results_0_daily_1["wind_direction_degree"]; 
  const char* results_0_daily_1_wind_speed = results_0_daily_1["wind_speed"];
  const char* results_0_daily_1_wind_scale = results_0_daily_1["wind_scale"];
  const char* results_0_daily_1_humidity = results_0_daily_1["humidity"]; 
  
  JsonObject results_0_daily_2 = results_0_daily[2];
  const char* results_0_daily_2_date = results_0_daily_2["date"];
  const char* results_0_daily_2_text_day = results_0_daily_2["text_day"];
  const char* results_0_daily_2_code_day = results_0_daily_2["code_day"];
  const char* results_0_daily_2_text_night = results_0_daily_2["text_night"];
  const char* results_0_daily_2_code_night = results_0_daily_2["code_night"];
  const char* results_0_daily_2_high = results_0_daily_2["high"]; 
  const char* results_0_daily_2_low = results_0_daily_2["low"]; 
  const char* results_0_daily_2_rainfall = results_0_daily_2["rainfall"];
  const char* results_0_daily_2_precip = results_0_daily_2["precip"]; 
  const char* results_0_daily_2_wind_direction = results_0_daily_2["wind_direction"]; 
  const char* results_0_daily_2_wind_direction_degree = results_0_daily_2["wind_direction_degree"]; 
  const char* results_0_daily_2_wind_speed = results_0_daily_2["wind_speed"];
  const char* results_0_daily_2_wind_scale = results_0_daily_2["wind_scale"]; 
  const char* results_0_daily_2_humidity = results_0_daily_2["humidity"]; 
  
  const char* results_0_last_update = results_0["last_update"]; 
  
  // 从以上信息中摘选几个通过串口监视器显示
  String results_0_daily_0_date_str = results_0_daily_0["date"].as<String>();
  String  results_0_daily_0_text_day_str = results_0_daily_0["text_day"].as<String>(); 
  int results_0_daily_0_code_day_int = results_0_daily_0["code_day"].as<int>(); 
  String results_0_daily_0_text_night_str = results_0_daily_0["text_night"].as<String>(); 
  int results_0_daily_0_code_night_int = results_0_daily_0["code_night"].as<int>(); 
  int results_0_daily_0_high_int = results_0_daily_0["high"].as<int>();
  int results_0_daily_0_low_int = results_0_daily_0["low"].as<int>();
  String results_0_last_update_str = results_0["last_update"].as<String>();
  String results_0_daily_0_humidity_str=results_0_daily_0["humidity"].as<String>();
  String  results_0_daily_0_wind_direction_str = results_0_daily_0["wind_direction"].as<String>();
  int results_0_daily_1_high_int = results_0_daily_1["high"].as<int>();
  int results_0_daily_1_low_int = results_0_daily_1["low"].as<int>();
  String results_0_daily_1_text_day_str = results_0_daily_1["text_day"].as<String>();




int results_0_daily_0_rainfall_int = results_0_daily_0["rainfall"].as<int>();
// String results_0_daily_0_wind_direction_str = results_0_daily_0["wind_direction"].as<String>();
 int    results_0_daily_0_wind_scale_int = results_0_daily_0["wind_scale"].as<int>();
// String results_0_daily_0_humidity_str = results_0_daily_0["humidity"].as<String>();
 
  Serial.println(F("======Today Weahter ======="));
  Serial.print(F("DATE: "));
  
  Serial.println(results_0_daily_0_date_str);
  Serial.print(F("Day Weather: "));
  Serial.print(results_0_daily_0_text_day_str);
  Serial.print(F(" "));
  Serial.println(results_0_daily_0_code_day_int);
  Serial.print(F("Night Weather: "));
  Serial.print(results_0_daily_0_text_night_str);
  Serial.print(F(" "));
  Serial.println(results_0_daily_0_code_night_int);
  Serial.print(F("High: "));
  Serial.println(results_0_daily_0_high_int);
  Serial.print(F("LOW: "));
  Serial.println(results_0_daily_0_low_int);
  Serial.print(F("Last Update: "));
  Serial.println(results_0_last_update_str);
  Serial.println(F("=============================="));
  Serial.print(F("rainfall "));
  Serial.println (results_0_daily_0_rainfall);
   Serial.print(F("windscale "));
  Serial.println (results_0_daily_1_wind_scale);
   Serial.print(F("humidity "));
  Serial.println (results_0_daily_0_humidity_str);
   Serial.print(F("winddirect "));
  Serial.println (results_0_daily_0_wind_direction_str);
  
//  display_1(results_0_daily_1_high_int,results_0_daily_1_low_int,results_0_daily_1_text_day_str);
  results_0_daily_1_high_int_old=results_0_daily_1_high_int;
  results_0_daily_1_low_int_old=results_0_daily_1_low_int;
  results_0_daily_1_text_day_str_old=results_0_daily_1_text_day_str;
  
 results_0_daily_0_rainfall_int_old=  results_0_daily_0_rainfall_int ;
 results_0_daily_0_wind_direction_str_old= results_0_daily_0_wind_direction_str ;
 results_0_daily_0_wind_scale_int_old=   results_0_daily_0_wind_scale_int;
 results_0_daily_0_humidity_str_old=results_0_daily_0_humidity_str;
 results_0_daily_0_high_int_old= results_0_daily_0_high_int ;
 results_0_daily_0_low_int_old= results_0_daily_0_low_int;
  
  
}else{
//  display_1(results_0_daily_1_high_int_old,results_0_daily_1_low_int_old,results_0_daily_1_text_day_str_old);
}


  
}
//----------预测明天天气----------
void display_1(int results_0_daily_1_high_int,int results_0_daily_1_low_int,String results_0_daily_1_text_day_str){
  //显示输出
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.setCursor(15, 14);
  u8g2.print("南京明天天气");
  
  u8g2.setFont(u8g2_font_logisoso24_tr);
  u8g2.setCursor(20, 46);
  u8g2.print(results_0_daily_1_low_int);
  u8g2.setCursor(56, 46);
  u8g2.print("~");
  u8g2.setCursor(75, 46);
  u8g2.print(results_0_daily_1_high_int);
  
  u8g2.setCursor(30, 62);
  u8g2.setFont(u8g2_font_unifont_t_chinese2);
  u8g2.print(results_0_daily_1_text_day_str);
  u8g2.sendBuffer();
}


/*-------- NTP 代码 ----------*/

const int NTP_PACKET_SIZE = 48;     // NTP时间在消息的前48个字节里
byte packetBuffer[NTP_PACKET_SIZE]; // 输入输出包的缓冲区

time_t getNtpTime()
{
    IPAddress ntpServerIP; // NTP服务器的地址

    while (Udp.parsePacket() > 0)
        ; // 丢弃以前接收的任何数据包
    Serial.println("Transmit NTP Request");
    // 从池中获取随机服务器
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            Serial.println("Receive NTP Response");
            isNTPConnected = true;
            Udp.read(packetBuffer, NTP_PACKET_SIZE); // 将数据包读取到缓冲区
            unsigned long secsSince1900;
            // 将从位置40开始的四个字节转换为长整型，只取前32位整数部分
            secsSince1900 = (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            Serial.println(secsSince1900);
            Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
            return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }
    }
    Serial.println("No NTP Response :-("); //无NTP响应
    isNTPConnected = false;
    return 0; //如果未得到时间则返回0
}

// 向给定地址的时间服务器发送NTP请求
void sendNTPpacket(IPAddress &address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;          // Stratum, or type of clock
    packetBuffer[2] = 6;          // Polling Interval
    packetBuffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    Udp.beginPacket(address, 123); //NTP需要使用的UDP端口号为123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

void oledClockDisplay()
{
    int years, months, days, hours, minutes, seconds, weekdays;
    years = year();
    months = month();
    days = day();
    hours = hour();
    minutes = minute();
    seconds = second();
    weekdays = weekday();
    Serial.printf("%d/%d/%d %d:%d:%d Weekday:%d\n", years, months, days, hours, minutes, seconds, weekdays);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_unifont_t_chinese2);
    /*u8g2.setCursor(0, 14);
    if (isNTPConnected)
    {
        if(timeZone>=0)
        {
            u8g2.print("Time(UTC+");
            u8g2.print(timeZone);
            u8g2.print(")");
        }
        else
        {
            u8g2.print("当前时间(UTC");
            u8g2.print(timeZone);
            u8g2.print(")");
        }
    }
    else
        u8g2.print("无网络!"); //如果上次对时失败，则会显示无网络
    */
    String currentTime = "";
    if (hours < 10)
        currentTime += 0;
    currentTime += hours;
    currentTime += ":";
    if (minutes < 10)
        currentTime += 0;
    currentTime += minutes;
    currentTime += ":";
    if (seconds < 10)
        currentTime += 0;
    currentTime += seconds;
    String currentDay = "";
    currentDay += years;
    currentDay += "/";
    if (months < 10)
        currentDay += 0;
    currentDay += months;
    currentDay += "/";
    if (days < 10)
        currentDay += 0;
    currentDay += days;

   // u8g2.setFont(u8g2_font_logisoso24_tr);
    u8g2.setFont(u8g2_font_maniac_tr);
    u8g2.setCursor(3, 44);
    u8g2.print(currentTime);
     
    u8g2.setCursor(3, 13);
    
    u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
    u8g2.print(currentDay);
 //   u8g2.drawFrame(0,0,256,64);
 //   u8g2.drawXBM(80, 48, 16, 16, xing);
 //   u8g2.drawXBM(205, 0, 16, 16, xing);gImage_mao
//=========================================================================================================================================================================
    u8g2.drawXBMP(pos_x+164, pos_y+17, cannon_width, cannon_height, mario_cannon);
 //   u8g2.drawXBMP(pos_x+228, pos_y+17, cannon_width, cannon_height, mario_cannon);
    pos_x = pos_x + X;
  // if (pos_x > OLED_WIDTH + cannon_width) {
       if (pos_x > OLED_WIDTH - 36) {
      pos_x = -cannon_width;
    };



    //  u8g2.drawXBM(165, 17, 36, 32,mario_cannon);
  //    u8g2.drawXBM(128, 17, 32, 32,gImage_mao);
 //   u8g2.setCursor(95, 62);
    u8g2.setCursor(72, 13);
  //  u8g2.print("期");
    if (weekdays == 1)
        u8g2.print("SUN");
    else if (weekdays == 2)
        u8g2.print("MON");
    else if (weekdays == 3)
        u8g2.print("TUE");
    else if (weekdays == 4)
        u8g2.print("WED");
    else if (weekdays == 5)
        u8g2.print("THU");
    else if (weekdays == 6)
        u8g2.print("FRI");
    else if (weekdays == 7)
        u8g2.print("SAT");
  //   u8g2.drawXBM(111, 49, 16, 16, liu);
  //      u8g2.drawXBM(237, 0, 16, 16, liu);
 //       u8g2.setCursor(120, 44);
   //     u8g2.setFont(u8g2_font_unifont_t_chinese2);
   //     u8g2.print(results_0_daily_1_high_int_old);

       
         u8g2.setFont(u8g2_font_unifont_t_weather);//tubiao wenduji 
         u8g2.drawGlyph(93, 14,0x0031);
         u8g2.drawGlyph(123, 14,0x003C);
          u8g2.drawGlyph(176, 13,0x0032);
         u8g2.setFont(u8g2_font_unifont_t_86);
         wind_direction(results_0_daily_0_wind_direction_str_old);


         u8g2.setFont(u8g2_font_siji_t_6x10);//tubiao wenduji 
     //    u8g2.drawGlyph(177, 48,0xe1af);
     //    u8g2.drawGlyph(177, 36,0xe1e9);

         
         u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);//tubiao 
        
         u8g2.setCursor(105, 13);
         u8g2.print(results_0_now_temperature_int_old);//wendu 
         u8g2.setCursor(140, 13);
         u8g2.print(results_0_daily_0_humidity_str_old);//shidu
          u8g2.setCursor(168, 13);
         u8g2.print(results_0_daily_0_wind_scale_int_old);
         u8g2.setCursor(190, 13);
         u8g2.print(results_0_daily_0_rainfall_int_old);
          
      //   u8g2.drawStr(190, 49, (ip2Str(WiFi.localIP())).c_str());//ip adress
         
      //  u8g2.drawVLine(114,0,50);
   
        u8g2.drawLine(0, 15, 256, 15);
        weathercase_0(results_0_now_code_int_old);
   //     weathercase_day
    //    weathercase_night
 
    u8g2.sendBuffer();

// if Weather Now:
//results_0_now_code_int
   
}
//  if (b=="SE"){
 //   
void wind_direction(String b)
{
  b= results_0_daily_0_wind_direction_str_old;
    if (b=="SE"){
    u8g2.drawGlyph(155, 13,0x2b76);
    }
   if (b=="NE"){
    u8g2.drawGlyph(155, 13,0x2b79);
    }
    if (b=="SW"){
    u8g2.drawGlyph(155, 13,0x2b77);
    }
   if (b=="NW"){
    u8g2.drawGlyph(154, 14,0x2b78);
    }
     if (b=="E"){
    u8g2.drawGlyph(155, 13,0x2b70);
    }
   if (b=="W"){
    u8g2.drawGlyph(154, 14,0x2b72);
    }
    if (b=="S"){
    u8g2.drawGlyph(155, 13,0x2b71);
    }
   if (b=="N"){
    u8g2.drawGlyph(155, 13,0x2b73);
    }
}



void weathercase_0(int a)
{
  a= results_0_now_code_int_old;
  //u8g2.drawStr( 0, 0, "Unicode");
 // u8g2.setFont(u8g2_font_unifont_t_symbols);
  //u8g2.setFontPosTop();
  //u8g2.drawUTF8(0, 24, "☀ ☁");
  switch(a) {
    case 0:
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 14,0x002E);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(222, 13);
       u8g2.print("SUNNY");//天气文字
       break;
    case 1:
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(200, 15,0x0029);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(222, 13);
       u8g2.print("CLEAR");//天气文字
       break;
    case 2:
    case 3:
      u8g2.drawUTF8(a*3, 36, "☂");
      break;
       case 4:  //cloooud4
       u8g2.setFont(u8g2_font_unifont_t_symbols);
       u8g2.drawGlyph(210, 15,0x2601);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(222, 13);
       u8g2.print("CLOUD");//天气文字
       break;
    case 5:
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(210, 15,0x0034);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(222, 13);
       u8g2.print("PCLOU");//天气文字
       break;
    case 6:
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(210, 15,0x0034);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(222, 13);
       u8g2.print("PCLOU");//天气文字
         break;
      case 7:
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(210, 15,0x0035);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(222, 13);
       u8g2.print("MCLOU");//天气文字
      break;
      case 8:
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(210, 15,0x0035);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(222, 13);
       u8g2.print("MCLOU");//天气文字
      break;
       case 9: //(YIN)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0035);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("OVCAS");//天气文字
      break;
      case 10: //(阵雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0036);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("SHOWR");//天气文字
      break;
      case 11: //(雷阵雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0039);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("THUND");//天气文字
      break;
    case 12: //(雷阵雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0039);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("THUND");//天气文字
     break;
      case 13: //(小雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0037);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("SRAIN");//天气文字
     break;
       case 14: //(中雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0037);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("MRAIN");//天气文字
     break;
       case 15: //(大雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0037);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("BRAIN");//天气文字
     break;
       case 16: //(暴雨雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("STORM");//天气文字
     break;
       case 17: //(大暴雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("HEAVY");//天气文字
     break;
        case 18: //(大暴雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("ULTRA");//天气文字
     break;
//   case 10:
//      u8g2.drawUTF8(a*3, 36, "☔");
//      break;
          case 19: //(冻雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("ICERA");//天气文字
     break;

           case 20: //(sLEET)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("SLEET");//天气文字
     break;
           case 21: //(阵雪)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003A);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("FLURR");//天气文字
     break;
        case 22: //(小雪)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("LSNOW");//天气文字
     break;
        case 23: //(中雨)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("MSNOW");//天气文字
     break;
        case 24: //(大雪)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("BSNOW");//天气文字
     break;
        case 25: //(BAOXUE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0038);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("STORM");//天气文字
     break;

            case 26: //(FUCHENG)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("DUST2");//天气文字
     break;
            case 27: //(BAOXUE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("SAND2");//天气文字
     break;
            case 28: //(BAOXUE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("STORM");//天气文字
     break;
            case 29: //(BAOXUE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("STORM");//天气文字
     break;
            case 30: //(GOG)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0025);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("FOGGY");//天气文字
     break;
         case 31: //(HAZE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x0025);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("HAZE2");//天气文字
     break;
         case 32: //(FENG)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("WINDY");//天气文字
     break;
         case 33: //(HAZE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("BLUST");//天气文字
     break;
         case 34: //(HAZE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("HURRI");//天气文字
     break;
         case 35: //(HAZE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("HAZE2");//天气文字
     break;
          case 36: //(HAZE)
       u8g2.setFont(u8g2_font_unifont_t_weather);
       u8g2.drawGlyph(204, 13,0x003B);
       u8g2.setFont(u8g2_font_prospero_bold_nbp_tr);
       u8g2.setCursor(219, 13);
       u8g2.print("TORND");//天气文字
     break;
   
  }
}

String ip2Str(IPAddress ip) { //IP地址转字符串
    String s = "";
    for (int i = 0; i < 4; i++) {
        s += i ? "." + String(ip[i]) : String(ip[i]);
    }
    return s;
}

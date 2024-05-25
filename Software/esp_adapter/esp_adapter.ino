
#include <stdint.h>
#include <Arduino.h>
#include <assert.h>
#include <IRrecv.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <time.h>
#include <map>
#if defined(ESP8266)
#include <Common.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#else
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#endif
// #include "BluetoothSerial.h"
#include "CEC_Device.h"


#include <FS.h>        // File System for Web Server Files
#include <LittleFS.h>  // This file system is used.

#include <CircularBuffer.hpp>  //For web console log

// local time zone definition
#define TIMEZONE "CST-8"

// The text of builtin files are in this header file
#include "builtinfiles.h"

//======================= feature enable ====================
#define IsSupportWebUIFullLog   1  // 1：所有串口log会同步到WEB Console,可能会导致某些lib的DEBUG log无法打印，开发调试过程请勿置1
                                   // 0: 仅IR Recv Dump log会同步到WEB Console
#define IsSupportMultiWiFi      1
#define IsESP8266_Heimei_Board  0 
#define MAXSubParameterCount    3  // 最大子字符串个数
#define MAXUartCommandWaitTime  3000



// ==================== begin of pin define ====================
#if defined(ESP8266)
  // Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
  #if IsESP8266_Heimei_Board
  const uint16_t kIrSendPin = 14;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
  const uint16_t kIrRecvPin = 5;  
  const uint16_t kRelayPin = 13;
  const uint16_t kCecPin =  12;
  const uint16_t kButtonPin = 2; 
  #else
  const uint16_t kIrSendPin = 2; 
  const uint16_t kIrRecvPin = 0;  
  const uint16_t kRelayPin = 4;
  const uint16_t kCecPin =  12;
  const uint16_t kButtonPin = 14; 
  #endif
#elif define(AirM2M_CORE_ESP32C3)
const uint16_t kIrSendPin = 4;  
// Note: GPIO 14 won't work on the ESP32-C3 as it causes the board to reboot.
const uint16_t kIrRecvPin = 8;  // 14 on a ESP32-C3 causes a boot loop.
const uint16_t kRelayPin = 9;
const uint16_t kCecPin =  7;
const uint16_t kButtonPin = 14; 
#endif


// ==================== begin of IR recv TUNEABLE PARAMETERS ====================
const uint16_t kCaptureBufferSize = 1024;
#if DECODE_AC
const uint8_t kTimeout = 50;
#else   
const uint8_t kTimeout = 15;
#endif 
const uint16_t kMinUnknownSize = 12;
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%
#define LEGACY_TIMING_INFO false   // Use turn on the save buffer feature for more complete capture coverage.

// ==================== begin of other PARAMETERS ====================

//uart PARAMETERS
const uint32_t kUartBaudRate = 115200;  

//button for SmartConfig & Restart
unsigned long debounceDelay = 500;    // 消除彈跳延遲時間

//wifi STA PARAMETERS
const char defaultWifiList[][2][32] = {
    {"ziroom709", "4001001111"},  // When IsSupportMultiWiFi=0, only the first item will be used
    {"TP-LINK_48D0", ""}
};
String wifiConfigFileName = "/wifi_config.txt";
int numWiFi = 0; // 当前WiFi配置数目


//wifi AP PARAMETERS
const char *APssid = "esp8266";
const char *APpassword = "Qq2017..";
IPAddress APip(192, 168, 1, 1);
IPAddress APgateway(192, 168, 1, 1);
IPAddress APsubnet(255, 255, 255, 0);

//ntp PARAMETERS
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800; // GMT +8 hours for East 8 zone
const int   daylightOffset_sec = 0; // No daylight saving time

//Panasonic
const uint16_t ksharpRcPowerAddress = 0x555A;
const uint32_t ksharpRcPowerCommand = 0xF118688E;

//RC5
uint64_t kVestelRcPowerdata = 0x4C;
uint64_t kVestelRcExitdata = 0x65;
uint64_t kVestelRcNetflixdata = 0x67;
uint64_t kVestelRcEnterdata = 0x75;

//CEC PARAMETERS
//please try https://github.com/CubeBall/CEC
#define CEC_DEVICE_TYPE CEC_Device::CDT_PLAYBACK_DEVICE
#define CEC_PHYSICAL_ADDRESS 0x2000

//HTTP PARAMETERS
const int http_port = 80;

// HttpUpdateServer PARAMETERS
const char* update_path = "/ota";
const char* update_username = "admin";
const char* update_password = "admin";

//Web Parameters
const char* www_username = "admin";
const char* www_password = "admin";

// ==================== begin of global class ====================
IRsend irsend(kIrSendPin);  // Set the GPIO to be used to sending the message.
decode_results IRRecvResults;     // Somewhere to store the results
IRrecv irrecv(kIrRecvPin, kCaptureBufferSize, kTimeout, true);
CEC_Device cec_device(kCecPin);
// BluetoothSerial SerialBT;
#if defined(ESP8266)
const char* HOSTNAME = "esp8266";
ESP8266WebServer httpserver(http_port);
  #if IsSupportMultiWiFi
  ESP8266WiFiMulti wifiMulti;
  #endif
ESP8266HTTPUpdateServer httpUpdater;
#else
const char* HOSTNAME = "esp32-c3";
WebServer httpserver(http_port);
  #if IsSupportMultiWiFi
  WiFiMulti wifiMulti;
  #endif
HTTPUpdateServer httpUpdater;
#endif
// Circular buffer for log storage
CircularBuffer<char, 2048> logBuffer;
class LoggingSerial : public HardwareSerial {
public:
    LoggingSerial(int uart_nr) : HardwareSerial(uart_nr) {}

    size_t write(uint8_t c) override {
        logBuffer.push(c); // Push to circular buffer
        return HardwareSerial::write(c); // Also write to hardware serial
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        for (size_t i = 0; i < size; i++) {
            logBuffer.push(buffer[i]);
        }
        return HardwareSerial::write(buffer, size); // Also write to hardware serial
    }
};
LoggingSerial LogSerial(0);

#if IsSupportWebUIFullLog
  #define Serial LogSerial
#endif

enum NetStatus {
  STA_MODE,
  STA_SC_MODE,
  AP_MODE,
  UNKNOWN_MODE
};

NetStatus currentNetStatus = UNKNOWN_MODE;

enum CommandType {
  Relay = 0x0,
  System = 0x1,
  wGPIO,
  rGPIO,
  mGPIO, 
  tGPIO,   
  IR_NEC,
  IR_Sony,
  IR_Sony38,
  IR_Panasonic,
  IR_RC5,
  IR_RC6,
  IR_Sharp,
  IR_JVC,
  IR_SAMSUNG,
  CEC_Send,
  IP_Get,
  UNKNOWN_Command
};

std::map<String, CommandType> commandMap = {
  {"wRelay", Relay},
  {"System", System},
  {"wGPIO", wGPIO},
  {"rGPIO", rGPIO},
  {"mGPIO", mGPIO},
  {"tGPIO", tGPIO},
  {"IR_NEC", IR_NEC},
  {"IR_Sony", IR_Sony},
  {"IR_Sony38", IR_Sony38},
  {"IR_Panasonic", IR_Panasonic},
  {"IR_RC5", IR_RC5},
  {"IR_RC6", IR_RC6},
  {"IR_Sharp", IR_Sharp},
  {"IR_JVC", IR_JVC},
  {"IR_SAMSUNG", IR_SAMSUNG},
  {"CEC_Send", CEC_Send},
  {"IP_Get", IP_Get}
};

CommandType stringToCommandType(const String& str) {
  if (commandMap.find(str) != commandMap.end()) {
    return commandMap[str];
  } else {
    return static_cast<CommandType>(-1);
  }
}

// ==================== Custom function declaration begins ====================
void printLocalTime();
void browseService(const char * service, const char * proto);
void mDNS_SD_Extended();
void dumpIR();
void handleTest();
void handleGraph();
void drawGraph();
void handleNotFound();
void handleSysInfo();
void onStationConnected(const WiFiEventStationModeConnected& evt);
void onStationDisconnected(const WiFiEventStationModeDisconnected& evt);
void handleWiFiEvent(WiFiEvent_t event);
void handleRedirect();
void handleListFiles();
void handleHttpAPI();
void handleUartCommand();
bool handleAPI(String protocol, int args_count, uint64_t args[MAXSubParameterCount], String message);
int parseKeyValue(String keyValue, uint64_t args[]);
void splitString(String str, String substrings[], int positions[], int count);
void findDollarPositions(String str, int& count, int positions[]);
uint64_t parseStringtoUint64(String str);
bool loadWifiConfigAndTryConnect(String fileName);
void saveWifiConfig(String fileName, const char* tmp_ssid , const char* tmp_password);
void handleAddWifi();
void checkButton();
void handleFileManagerShow();
void handleFileManagerDelete();
void handleFileManagerUpload();

// ==================== Main function declaration begins ====================

void setup() {
  //disableLoopWDT();
  //Serial.begin(kUartBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.begin(kUartBaudRate, SERIAL_8N1);
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);
  // Perform a low level sanity checks that the compiler performs bit field
  // packing as we expect and Endianness is as we expect.
  assert(irutils::lowLevelSanityCheck() == 0);
  Serial.println("");
  Serial.printf("The last reset reason: %s \n", ESP.getResetReason().c_str());
  Serial.printf("Mounting the filesystem...\n");
  if (!LittleFS.begin()) {
    Serial.printf("could not mount the filesystem...\n");
    delay(2000);
  }

  Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kIrRecvPin);
  // SerialBT.begin("ESP32BT"); //Bluetooth device name

  cec_device.Initialize(CEC_PHYSICAL_ADDRESS, CEC_DEVICE_TYPE, true); // CEC init //Promiscuous mode

  pinMode(kButtonPin, INPUT_PULLUP);
  pinMode(kRelayPin, OUTPUT);
  digitalWrite(kRelayPin, HIGH);  // Relay Pin init

#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif                                        // DECODE_HASH
  irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
  irrecv.enableIRIn();                        // Start the receiver
  irsend.begin();
  
  if (!LittleFS.exists(wifiConfigFileName)) {  // Write Default WifiList to ConfigFile 
    uint16_t defaultWifiListCount = sizeof(defaultWifiList) / sizeof(defaultWifiList[0]);
    Serial.printf("The wifiConfigFile lost, we will create and write %u defaultWifiList to wifiConfigFile %s\n", defaultWifiListCount, wifiConfigFileName.c_str());
    for (uint16_t i = 0; i < defaultWifiListCount; i++) {
        saveWifiConfig(wifiConfigFileName, defaultWifiList[i][0], defaultWifiList[i][1]);
    }
  }

  Serial.println("Starting loading wifi config & connect to  wifi ...");
  WiFi.mode(WIFI_STA);                        // start to Connect to WiFi
  WiFi.setHostname(HOSTNAME);
  WiFi.onStationModeConnected(&onStationConnected);
  WiFi.onStationModeDisconnected(&onStationDisconnected);
  WiFi.onEvent(&handleWiFiEvent);

  if(!loadWifiConfigAndTryConnect(wifiConfigFileName) || WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect wifi, We will start AP");
    WiFi.printDiag(Serial);
    currentNetStatus = AP_MODE;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(APip, APgateway, APsubnet);
    WiFi.softAP(APssid, APpassword);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    Serial.println();
  } else {
    currentNetStatus = STA_MODE;
    Serial.print("setup(): WiFi Connected: ");
    Serial.println(WiFi.SSID());
    Serial.print("setup(): IP Address: ");
    Serial.println(WiFi.localIP());
  
    // Init ntp and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
  }

  // Set mDNS server
  if (MDNS.begin(HOSTNAME)) 
  { 
    Serial.println("MDNS responder started"); 
  } else {
    Serial.println("MDNS responder failed");
  }

  // Set httpUpdater server
  httpUpdater.setup(&httpserver, update_path, update_username, update_password);

  // Start the httpserver
    // UPLOAD and DELETE of files in the file system using a request handler.
  //httpserver.addHandler(new FileServerHandler());
  httpserver.on("/test", HTTP_GET, handleTest);
  httpserver.on("/api", HTTP_GET, handleHttpAPI);
  httpserver.on("/sysinfo", HTTP_GET, handleSysInfo);
  httpserver.on("/logs", HTTP_GET, handleGetLogs);
  httpserver.on("/pic", HTTP_GET, handleGraph);
  httpserver.on("/rand.svg", HTTP_GET, drawGraph);
  // register a redirect handler when only domain name is given.
  httpserver.on("/", HTTP_GET, handleRedirect);
  httpserver.on("/list", HTTP_GET, handleListFiles);
   // serve a built-in htm page
  httpserver.on("/files", HTTP_GET, handleFileManagerShow);
  httpserver.on("/files", HTTP_DELETE, handleFileManagerDelete);
  //upload will be called multiple times and we have to return 200 as soon as possible, 
  //otherwise the upload will not continue and we will only get the UPLOAD_FILE_END status and an incomplete file
  httpserver.on("/files", HTTP_POST, [](){ httpserver.send(200);}, handleFileManagerUpload);  
  httpserver.on("/wifi", handleAddWifi);
  httpserver.onNotFound(handleNotFound);
  //enable CORS header in webserver results
  httpserver.enableCORS(true);
  //enable ETAG header in webserver results from serveStatic handler
  httpserver.enableETag(true);
  // serve all static files
  httpserver.serveStatic("/", LittleFS, "/");

  httpserver.begin();
  Serial.println("Http Server started");

  // Add http service to MDNS-SD(service discovery).
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser for OTA\n", HOSTNAME, update_path);

  
}

void loop() {
  if(currentNetStatus == AP_MODE)
      checkButton();
  if (Serial.available() > 0) {
    handleUartCommand();
  }
  cec_device.Run();
  httpserver.handleClient();
  MDNS.update();
  dumpIR();
}

// ========================Custom function begins==============================
// 用于将日志信息添加到循环缓冲区的函数
void logToWebConsole(const String& message) {
    for (size_t i = 0; i < message.length(); i++) {
        logBuffer.push(message[i]);
    }
}
String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void checkButton() {
  unsigned long pressStartTime = 0;
  unsigned long pressDuration = 0;
  if (digitalRead(kButtonPin) == LOW) {
    pressStartTime = millis(); 
    Serial.println("Button pressed! Keep holding...");
    while (digitalRead(kButtonPin) == LOW) {
    }
    pressDuration = millis() - pressStartTime;
    Serial.print("Button released! Press duration: ");
    Serial.print(pressDuration);
    Serial.println(" ms");
    if (pressDuration > debounceDelay) {
      if(pressDuration < 5000) { // 短按
        if(currentNetStatus == AP_MODE) {
	        WiFi.softAPdisconnect(true);
	        WiFi.mode(WIFI_STA);
	        Serial.println("Starting SmartConfig...");
	        WiFi.beginSmartConfig();
		      currentNetStatus = STA_SC_MODE;
          }
      } else { // 長按
        Serial.println("Restarting...");
        delay(3000); // 等待確保重啟指令被處理
        ESP.restart();
      }
    }
  }
}

uint64_t parseStringtoUint64(String str) {
    uint64_t value = 0;

    if (str.startsWith("0x") || str.startsWith("0X")) {
        value = strtoull(str.substring(2).c_str(), NULL, 16);
    } else {
        value = strtoull(str.c_str(), NULL, 10);
    }

    return value;
}

void findDollarPositions(String str, int& count, int positions[]) {
    count = 0;
    for (unsigned int i = 0; i < str.length(); i++) {
        if (str.charAt(i) == '$') {
            if (count < MAXSubParameterCount - 1) {  // $符号的最大数量是最大子字符串个数 - 1
                positions[count] = i;
                count++;
            }
        }
    }
}

void splitString(String str, String substrings[], int positions[], int count) {
    int start = 0;
    for (int i = 0; i < count; i++) {
        substrings[i] = str.substring(start, positions[i]);
        start = positions[i] + 1;
    }
    substrings[count] = str.substring(start);
}

int parseKeyValue(String keyValue, uint64_t args[]) {
    if (keyValue.length() == 0) {  // 处理空字符串的情况
        args[0] = 0;
        return 1;
    }

    int positions[MAXSubParameterCount - 1];
    int count = 0;
    findDollarPositions(keyValue, count, positions);
    String substrings[MAXSubParameterCount]; // 最多可以有MAXSubParameterCount个子字符串
    splitString(keyValue, substrings, positions, count);

    int i;
    for (i = 0; i <= count; i++) {
        args[i] = parseStringtoUint64(substrings[i]);
    }
    for (; i < MAXSubParameterCount; i++) {
        args[i] = UINT64_MAX; // 将剩余的元素设置为特殊值
    }
    return count + 1; // 返回实际解析出的子字符串数量
}


void onStationConnected(const WiFiEventStationModeConnected& evt) {
  Serial.print("Wifi connected: ");
  //Serial.println(macToString(evt.mac));
  Serial.print(evt.ssid);
  Serial.print(" bssid:");
  Serial.println(macToString(evt.bssid));
  Serial.print(" channel:");
  Serial.println(evt.channel);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void onStationDisconnected(const WiFiEventStationModeDisconnected& evt) {
  Serial.print("Wifi disconnected: ");
  Serial.print(evt.ssid);
  Serial.print(" bssid:");
  Serial.println(macToString(evt.bssid));
  Serial.print(" DisconnectReason:");
  Serial.print(evt.reason);
}


void handleWiFiEvent(WiFiEvent_t event){
    Serial.printf("Wifi Event: %d\n", event);
    switch(event) {
        case WIFI_EVENT_STAMODE_CONNECTED:   
            // Init ntp and get the time
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  
            //printLocalTime();                      
            break;
         case WIFI_EVENT_STAMODE_DISCONNECTED:
            break;
        case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
            break;
        case WIFI_EVENT_STAMODE_GOT_IP:
            Serial.print("Event: WiFi Connected: ");
            Serial.println(WiFi.SSID());
            Serial.print("Event: IP Address: ");
            Serial.println(WiFi.localIP());
            if(currentNetStatus == STA_SC_MODE) {
              // Get and save the received WiFi credentials
              String tmp_ssid = WiFi.SSID();
              String tmp_password = WiFi.psk();
              Serial.printf("SmartConfig received, ssid:%s, password:%s\n", tmp_ssid.c_str(), tmp_password.c_str());
              saveWifiConfig(wifiConfigFileName, tmp_ssid.c_str(), tmp_password.c_str()); 
              WiFi.stopSmartConfig();
              currentNetStatus = STA_MODE;
            }
            break;
        case WIFI_EVENT_STAMODE_DHCP_TIMEOUT:
            break;
        case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
            break;
        case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
            break;
        case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
            break;
        case WIFI_EVENT_MODE_CHANGE:
            break;
        case WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP:
            break;
        default:
            break;
   }
}


void saveWifiConfig(String fileName, const char* tmp_ssid , const char* tmp_password) {
    File configFile = LittleFS.open(fileName, "a");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return;
    }
    configFile.print(tmp_ssid);
    configFile.print(",");
    configFile.println(tmp_password);

    configFile.close();
}



bool loadWifiConfigAndTryConnect(String fileName) {
    bool hasItems = false;
    File configFile = LittleFS.open(fileName, "r");
    if (!configFile) {
        Serial.println("Failed to open config file for loading");
        return false;
    }

    numWiFi = 0;

    while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        if (line.length() > 0) {
            line.trim();
            int separatorIndex = line.indexOf(',');
            if (separatorIndex != -1) {
                String ssid = line.substring(0, separatorIndex);
                String psk  = line.substring(separatorIndex + 1);
#if IsSupportMultiWiFi
                wifiMulti.addAP(ssid.c_str(), psk.c_str());
#else
                WiFi.begin(ssid.c_str(), psk.c_str());   //only use the first item
                break;
#endif
                hasItems = true;
                numWiFi++;
            }
        }
    }
    if (!hasItems) {
        Serial.println("Failed to load, There are no valid items in the config file");
        configFile.close();
        return false;
    }
#if IsSupportMultiWiFi
    Serial.printf("Try connecting to Multi Wifi, numWiFi=%d...\r\n", numWiFi);

    // Wait for wifi connection or timeout
    unsigned long startAttemptTime = millis();
    while (wifiMulti.run() != WL_CONNECTED && millis() - startAttemptTime < (long unsigned int)numWiFi*2*5000) {
        delay(500);
        Serial.print(".");
    }
#else
    Serial.printf("Try connecting to %s \r\n", wifiList[0][0]);
    // Wait for wifi connection or timeout
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(500);
        Serial.print(".");
    }
#endif
    configFile.close();
    return true;
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  char timeStr[64]; // 创建一个足够大的缓冲区来存储格式化的时间
  strftime(timeStr, sizeof(timeStr), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  Serial.println(timeStr);
}


void browseService(const char * service, const char * proto){
    Serial.printf("Browsing for service _%s._%s.local. ... ", service, proto);
    int n = MDNS.queryService(service, proto);
    if (n == 0) {
        Serial.println("no services found");
    } else {
        Serial.print(n);
        Serial.println(" service(s) found");
        for (int i = 0; i < n; ++i) {
            // Print details for each service found
            Serial.print("  ");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(MDNS.hostname(i));
            Serial.print(" (");
            Serial.print(MDNS.IP(i));
            Serial.print(":");
            Serial.print(MDNS.port(i));
            Serial.println(")");
        }
    }
    Serial.println();
}


void mDNS_SD_Extended() {
    browseService("http", "tcp");
    delay(1000);
    browseService("arduino", "tcp");
    delay(1000);
    browseService("workstation", "tcp");
    delay(1000);
    browseService("smb", "tcp");
    delay(1000);
    browseService("afpovertcp", "tcp");
    delay(1000);
    browseService("ftp", "tcp");
    delay(1000);
    browseService("ipp", "tcp");
    delay(1000);
    browseService("printer", "tcp");
    delay(1000);
}

void handleGetLogs() {
  String logData;
  while (!logBuffer.isEmpty()) {
      logData += logBuffer.shift();
  }
  httpserver.send(200, "text/plain", logData);
}

// This function is called when the WebServer was requested without giving a filename.
// This will redirect to the file index.htm when it is existing otherwise to the built-in $upload.htm page
void handleRedirect() {
  Serial.println("Redirect...");
  String url = httpserver.uri();
  if (url.endsWith("/")) {
    url += "index.html";
  }
  Serial.print("Url:");
  Serial.println(url);

  if (!LittleFS.exists(url)) {
    if(currentNetStatus == STA_MODE) {
      url = "/file";
    } else {
      url = "/wifi";
    } 
  }
  //httpserver.redirect(url);

  httpserver.sendHeader("Location", url, true);
  httpserver.send(302);
}  // handleRedirect()


// This function is called when the WebServer was requested to list all existing files in the filesystem.
// a JSON array with file information is returned.
void handleListFiles() {
  Dir dir = LittleFS.openDir("/");
  String result;

  result += "[\n";
  while (dir.next()) {
    if (result.length() > 4) { result += ","; }
    result += "  {";
    result += " \"name\": \"" + dir.fileName() + "\", ";
    result += " \"size\": " + String(dir.fileSize()) + ", ";
    result += " \"time\": " + String(dir.fileTime());
    result += " }\n";
    // jc.addProperty("size", dir.fileSize());
  }  // while
  result += "]";
  httpserver.sendHeader("Cache-Control", "no-cache");
  httpserver.send(200, "text/javascript; charset=utf-8", result);
}  // handleListFiles()


bool checkUserAuthentication() {
  if (!httpserver.authenticate(www_username, www_password)) {
    httpserver.requestAuthentication();
    return false;
  }
  return true;
}

  // if (!checkUserAuthentication()) {
  //   return; // End the request if not authenticated
  // }

void handleAddWifi() {
  if (httpserver.method() == HTTP_GET) {
    // 处理GET请求
    httpserver.send(200, "text/html", FPSTR(WiFiHtml));
  } else if (httpserver.method() == HTTP_POST) {
    String ssid = httpserver.arg("ssid");
    String password = httpserver.arg("password");
    Serial.printf("Recv ssid:%s password:%s\n" , ssid.c_str(), password.c_str());
    saveWifiConfig(wifiConfigFileName, ssid.c_str(), password.c_str());
    httpserver.send(200, "text/plain", "Add Wifi item Successfully");
    // delay(5000);
    // ESP.restart();
  }
}

void handleFileManagerShow() {
  httpserver.send(200, "text/html", FPSTR(FileManagerHtml));
}
void handleFileManagerDelete() {
  String filename = httpserver.arg("delfile"); // Assuming the file name is passed as a URL argument
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  if (LittleFS.remove(filename)) {
    Serial.println("File Deleted Successfully");
    httpserver.send(200, "text/plain", "File Deleted Successfully");
  } else {
    Serial.println("File Deletion Failed");
    httpserver.send(500, "text/plain", "File Deletion Failed");
  }
}

void handleFileManagerUpload() {
  static fs::File file;
  static String filename;
  HTTPUpload& upload = httpserver.upload();
  filename = upload.filename;
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  Serial.print("Handle File Upload Name: ");
  Serial.println(filename);
  if (upload.status == UPLOAD_FILE_START) {
    Serial.println("start uploading");
    file = LittleFS.open(filename, "w"); 
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (file) {
      Serial.println("still uploading");
      file.write(upload.buf, upload.currentSize); 
    } else {
      Serial.println("Failed to open file for appending");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!file) {      //for handle too small file
      Serial.println("handle too small file case");
      file = LittleFS.open(filename, "w");
      if (file) {
        file.write(upload.buf, upload.currentSize); 
      } else {
        Serial.println("Failed to open file for writing at end");
      }
    }
    if (file) {
      file.close(); // Close the file when upload is finished
    }
    Serial.println("Upload Finished");
  }
}


void handleUartCommand() {

  String data = "";
  unsigned long startTime = millis();
  String protocol = "";
  int args_count = 0;
  uint64_t args[MAXSubParameterCount] = {0};
  bool receivedNewline = false;

  //loop for recv command until timeout or newline
  while (millis() - startTime < MAXUartCommandWaitTime && !receivedNewline) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        receivedNewline = true;
      } else {
        data += c;
      }
    }
  }

  if (!receivedNewline) {
    Serial.println("Receiving input timed out, please enter a complete command within" + String(MAXUartCommandWaitTime) + "ms and press Enter.");
    return; 
  }
  
  data.trim(); 

  if(data.length() == 0)
    return;

  int index = 0;
  args_count = 0;
  int lastSpaceIndex = -1;
  while (index < data.length()) {
    int spaceIndex = data.indexOf(' ', lastSpaceIndex + 1);
    if (spaceIndex == -1) {
      spaceIndex = data.length();
    }

    String param = data.substring(lastSpaceIndex + 1, spaceIndex);
    param.trim(); 
    if (args_count == 0) {
      protocol = param;
    } else {
      args[args_count - 1] = parseStringtoUint64(param);
    }
    args_count++;
    lastSpaceIndex = spaceIndex;
    if (spaceIndex == data.length()) {
      break; 
    }
  }
  args_count = args_count - 1;  //exclude protocol

  Serial.printf("handleUartCommand data=%s protocol=%s args_count=%d \n", data.c_str(), protocol.c_str(), args_count);
  for (int i = 0; i < args_count; i++) {
    Serial.printf("args[%d]: 0x%llx", i, args[i]);
  }
  Serial.println("");
  String message = "";
  if (handleAPI(protocol, args_count, args, message)){
    Serial.printf("Request processed. %s\n", message.c_str());
  } else {
    Serial.println("Unsupport protocol, please check! eg.\"wGPIO 24 1\" to write GPIO24 to high");
  }
  return ;
}

void handleHttpAPI() {
  if (httpserver.hasArg("protocol") && httpserver.hasArg("keyValue")) {
      String protocol = httpserver.arg("protocol");
      String keyValue = httpserver.arg("keyValue");
      uint64_t args[MAXSubParameterCount];
      int args_count = parseKeyValue(keyValue, args);
      Serial.printf("handleHttpAPI protocol=%s keyValue=%s args_count=%d \n", protocol.c_str(), keyValue.c_str(), args_count);
      for (int i = 0; i < args_count; i++) {
          if (args[i] != UINT64_MAX) {
              Serial.printf("Value %d: 0x%llx\n", i, args[i]);
          } else {
              Serial.println("Value " + String(i) + ": Invalid/Unused");
          }
      }
      String message = "";
      if (handleAPI(protocol, args_count, args, message)){
        httpserver.send(200, "text/plain", "Request processed");
        Serial.println("Request processed");
      } else {
        httpserver.send(400, "text/plain", "Unsupport protocol, please check!");
        Serial.println("Unsupport protocol, please check!");
        return;
      }
  } else {
      httpserver.send(400, "text/plain", "Missing or invalid  parameter");
      Serial.println("Missing or invalid  parameter");
  }
}

bool handleAPI(String protocol, int args_count, uint64_t args[MAXSubParameterCount], String message) {

  switch (stringToCommandType(protocol)) {
    case Relay:
        digitalWrite(kRelayPin, args[0]);
        break;
    case System:
        message = "ESP Reboot...";
        ESP.restart(); // TODO: Add more system functions
        break;
    case rGPIO:
        message = String(digitalRead(args[0])); // TODO: We need to return the result
        break;
    case wGPIO:
        digitalWrite(args[0], LOW);
        break;
    case mGPIO:
        pinMode(args[0], args[1]);
        break;
    case tGPIO:
        if (digitalRead(args[0]) == HIGH)
            digitalWrite(args[0], LOW);
        else
            digitalWrite(args[0], HIGH);
        break;
    case IR_Panasonic:
        if (args_count > 1)
            irsend.sendPanasonic(args[0], args[1]);
        break;
    case IR_RC5:
        irsend.sendRC5(args[0]);
        break;
    case IR_NEC:
        irsend.sendNEC(args[0]);
        break;
    /*
    case IR_Sony:
        irsend.sendSony(args[0]);
        break;
    case IR_Sony38:
        irsend.sendSony38(args[0]);
        break;
    case IR_RC6:
        irsend.sendRC6(args[0]);
        break;
    case IR_Sharp:
        irsend.Sharp(args[0]);
        break;
    case IR_JVC:
        irsend.JVC(args[0]);
        break;
    case IR_SAMSUNG:
        irsend.sendSAMSUNG(args[0]);
        break;      */
    case CEC_Send:
        unsigned char buffer[3];
        //args[0]
        buffer[0] = 0x36;
			  cec_device.TransmitFrame(4, buffer, 1);
        break;
    case IP_Get:
        message = WiFi.localIP().toString();
        break;
    default:
        // Handle unknown protocol
        return false;
        break;
  }
  return true;
}

// This function is called when the sysInfo service was requested.
void handleSysInfo() {
  String result;
  uint32_t free;
  uint32_t max;
  uint8_t frag;
  ESP.getHeapStats(&free, &max, &frag);
  FSInfo fs_info;
  LittleFS.info(fs_info);

  result += "{\n";
  //system_print_meminfo(); // 这个函数输出的信息可能不适合直接放入JSON字符串中
  //system_show_malloc();
  result += "  \"flashideSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"flashrealSize\": " + String(ESP.getFlashChipRealSize()) + ",\n";
  result += "  \"flashideMode\": " + String(ESP.getFlashChipMode()) + ",\n";
  result += "  \"flashChipVendorId\": " + String(ESP.getFlashChipVendorId(), HEX) + ",\n";
  result += "  \"flashChipId\": " + String(ESP.getFlashChipId(), HEX) + ",\n";
  result += "  \"flashChipSpeed\": " + String(ESP.getFlashChipSpeed()) + ",\n";
  result += "  \"getCycleCount\": " + String(ESP.getCycleCount()) + ",\n";
  result += "  \"SketchSize\": " + String(ESP.getSketchSize()) + ",\n";
  result += "  \"FreeSketchSpace\": " + String(ESP.getFreeSketchSpace()) + ",\n";
  result += "  \"getSketchMD5\": " + String(ESP.getSketchMD5()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  result += "  \"system_get_free_heap_size()\": " + String(system_get_free_heap_size()) + ",\n";
  result += "  \"getHeapFragmentation\": " + String(ESP.getHeapFragmentation()) + ",\n";
  result += "  \"getHeapStats : \"free: " +  String(free) + "- max: " +  String(max) + " - frag: "  +  String(frag) + " <- " + "\",\n";;
  result += "  \"system_get_chip_id()\": \"0x" + String(system_get_chip_id(), HEX) + "\",\n";
  result += "  \"system_get_sdk_version()\": \"" + String(system_get_sdk_version()) + "\",\n";
  result += "  \"getCoreVersion()\": \"" + String(ESP.getCoreVersion()) + "\",\n";
  result += "  \"getFullVersion()\": \"" + String(ESP.getFullVersion()) + "\",\n";
  result += "  \"system_get_boot_version()\": " + String(system_get_boot_version()) + ",\n";
  result += "  \"system_get_userbin_addr()\": \"0x" + String(system_get_userbin_addr(), HEX) + "\",\n";
  result += "  \"system_get_boot_mode()\": \"" + String(system_get_boot_mode()) + "\",\n";
  result += "  \"system_get_cpu_freq()\": " + String(system_get_cpu_freq()) + ",\n";
  result += "  \"wifi_get_channel()\": " + String(wifi_get_channel()) + ",\n";
  result += "  \"system_get_flash_size_map()\": \"" + String(system_get_flash_size_map()) + "\"\n";
  result += "  \"fsTotalBytes\": " + String(fs_info.totalBytes) + ",\n";
  result += "  \"fsUsedBytes\": " + String(fs_info.usedBytes) + ",\n";
  result += "}";

  httpserver.sendHeader("Cache-Control", "no-cache");
  httpserver.send(200, "text/javascript; charset=utf-8", result);
}  // handleSysInfo()


void handleGraph() {
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  StreamString temp;
  temp.reserve(500);  // Preallocate a large chunk to avoid memory fragmentation
  temp.printf("\
<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <img src=\"/rand.svg\" />\
  </body>\
</html>",
              hr, min % 60, sec % 60);
  httpserver.send(200, "text/html", temp.c_str());
}

void drawGraph() {
  String out;
  out.reserve(2600);
  char temp[70];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10) {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  httpserver.send(200, "image/svg+xml", out);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpserver.uri();
  message += "\nMethod: ";
  message += (httpserver.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpserver.args();
  message += "\n";

  for (uint8_t i = 0; i < httpserver.args(); i++) { message += " " + httpserver.argName(i) + ": " + httpserver.arg(i) + "\n"; }

  httpserver.send(404, "text/plain", message);
}

void handleTest() {
  //ClientType client = httpserver.client();
  Serial.println(" ");
  Serial.print("Url:");
  Serial.println(httpserver.uri());
  int args_num = httpserver.args();
  Serial.print("has args:");
  Serial.println(args_num);
  if(httpserver.args())
  {
    for(int i=0; i < args_num; i++ )
    {
      Serial.print("arg:");
      Serial.print(httpserver.argName(i));
      Serial.print("=");
      Serial.println(httpserver.arg(i));
    }
  } else {
    httpserver.send(200, "text/html", "<h1>You are connected</h1><br> \
                                      Click <a href=\"/?H\">here</a> to turn the Relay on pin 4 on.<br> \
                                      Click <a href=\"/?L\">here</a> to turn the Relay on pin 4 off.<br> \
                                      Click <a href=\"/trigger\">here</a> to change the Relay on pin 4.<br>");
  }
}

void dumpIR()
{
    // Check if the IR code has been received.
  if (irrecv.decode(&IRRecvResults)) {
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
    // Check if we got an IR message that was to big for our capture buffer.
    if (IRRecvResults.overflow)
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
    // Display the library version the message was captured with.
    Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");
    // Display the tolerance percentage if it has been change from the default.
    if (kTolerancePercentage != kTolerance)
      Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);
    // Display the basic output of what we found.
    Serial.print(resultToHumanReadableBasic(&IRRecvResults));
    // Display any extra A/C info if we have it.
    String description = IRAcUtils::resultAcToString(&IRRecvResults);
    if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);
      //yield();  // Feed the WDT as the text output can take a while to print.
#if LEGACY_TIMING_INFO
    // Output legacy RAW timing info of the result.
    Serial.println(resultToTimingInfo(&IRRecvResults));
    //yield();  // Feed the WDT (again)
#endif  // LEGACY_TIMING_INFO
    // Output the results as source code
    Serial.println(resultToSourceCode(&IRRecvResults));
#if !IsSupportWebUIFullLog
     logToWebConsole(resultToSourceCode(&IRRecvResults));
#endif
    Serial.println();  // Blank line between entries
    //yield();             // Feed the WDT (again)
  }
}








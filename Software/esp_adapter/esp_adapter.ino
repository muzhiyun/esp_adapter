/*
    This sketch establishes a TCP connection to a "quote of the day" service.
    It sends a "hello" message, and then prints received data.
*/
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#include <IRsend.h>

const uint16_t kIrLed = 2;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.


#ifndef STASSID
#define STASSID "ziroom709"
#define STAPSK "4001001111"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

// const char* host = "djxmmx.net";
// const uint16_t port = 17;

#ifdef ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 10;  // 14 on a ESP32-C3 causes a boot loop.
#else  
const uint16_t kRecvPin = 12;
#endif  

const uint16_t kCaptureBufferSize = 1024;

#if DECODE_AC
// Some A/C units have gaps in their protocols of ~40ms. e.g. Kelvinator
// A value this large may swallow repeats of some protocols
const uint8_t kTimeout = 50;
#else   // DECODE_AC
// Suits most messages, while not swallowing many repeats.
const uint8_t kTimeout = 15;
#endif  // DECODE_AC


const uint16_t kMinUnknownSize = 12;

const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%

#define LEGACY_TIMING_INFO false

IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results;  // Somewhere to store the results

volatile bool newData = false;
String logData;

void handleSSE() {
  server.sendHeader("Content-Type", "text/event-stream");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/event-stream", ":ok\n\n");

  if (newData) {
    server.sendContent("data: " + logData + "\n\n");
    newData = false;
  }
}

void serialEvent() {
  while (Serial.available()) {
    char c = Serial.read();
    logData += c;
    if (c == '\n') {
      newData = true;
    }
  }
}



uint8_t acData[] = {
    0x83,0x06,0x04,0x92,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x16,0x00,0x25,0x00,0x00,0x20,0x14,0x11};

void setup() {
  irsend.begin();
  Serial.begin(115200);
  pinMode(4, OUTPUT);


  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  //server.on("/events", HTTP_GET, handleSSE);
  server.begin();
  Serial.println("HTTP server started");

  #if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif  // DECODE_HASH
  irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
  irrecv.enableIRIn();  // Start the receiver
  Serial.println("HTTP server finish");

}


void handleRoot() {
  Serial.println("handleRoot");
  server.send(200, "text/html", "<h1>You are connected yaning</h1>");
  Serial.println("a Samsung A/C state from IRrecvDumpV2");
  irsend.sendWhirlpoolAC(acData);
  Serial.printf("somebody connected http\n");
  if(digitalRead(4) == HIGH)
    digitalWrite(4, LOW);  // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is active low on the ESP-01)
  else               
    digitalWrite(4, HIGH);  // Turn the LED off by making the voltage HIGH
}



void loop() {
  server.handleClient();
  // if (irrecv.decode(&results)) {
  //   // Display a crude timestamp.
  //   uint32_t now = millis();
  //   Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
  //   // Check if we got an IR message that was to big for our capture buffer.
  //   if (results.overflow)
  //     Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
  //   // Display the library version the message was captured with.
  //   Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");
  //   // Display the tolerance percentage if it has been change from the default.
  //   if (kTolerancePercentage != kTolerance)
  //     Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);
  //   // Display the basic output of what we found.
  //   Serial.print(resultToHumanReadableBasic(&results));
  //   // Display any extra A/C info if we have it.
  //   String description = IRAcUtils::resultAcToString(&results);
  //   if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);
  //   yield();  // Feed the WDT as the text output can take a while to print.
  // }
}


// void loop() {
  // static bool wait = false;

  // Serial.print("connecting to ");
  // Serial.print(host);
  // Serial.print(':');
  // Serial.println(port);

  // // Use WiFiClient class to create TCP connections
  // WiFiClient client;
  // if (!client.connect(host, port)) {
  //   Serial.println("connection failed");
  //   delay(5000);
  //   return;
  // }

  // // This will send a string to the server
  // Serial.println("sending data to server");
  // if (client.connected()) 
  // { client.println("hello from ESP8266"); }

  // // wait for data to be available
  // unsigned long timeout = millis();
  // while (client.available() == 0) {
  //   if (millis() - timeout > 5000) {
  //     Serial.println(">>> Client Timeout !");
  //     client.stop();
  //     delay(60000);
  //     return;
  //   }
  // }

  // // Read all the lines of the reply from server and print them to Serial
  // Serial.println("receiving from remote server");
  // // not testing 'client.connected()' since we do not need to send data here
  // while (client.available()) {
  //   char ch = static_cast<char>(client.read());
  //   Serial.print(ch);
  // }

  // // Close the connection
  // Serial.println();
  // Serial.println("closing connection");
  // client.stop();

  // if (wait) {
  //   delay(300000);  // execute once every 5 minutes, don't flood remote service
  // }
  // wait = true;
// }

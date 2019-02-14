#include "credentials.h"

#include <gfxfont.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>


#include <WiFi.h>
 #include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <WebSocketsServer.h>
#include <Adafruit_SSD1306.h>

// Pin definetion of WIFI LoRa 32
// HelTec AutoMation 2017 support@heltec.cn
#define SCK     5    // GPIO5  -- SX127x's SCK
#define MISO    19   // GPIO19 -- SX127x's MISO
#define MOSI    27   // GPIO27 -- SX127x's MOSI
#define SS      18   // GPIO18 -- SX127x's CS
#define RST     14   // GPIO14 -- SX127x's RESET
#define DI00    26   // GPIO26 -- SX127x's IRQ(Interrupt Request)

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     16
#define MARGIN 5

// Globals
WebSocketsServer webSocket = WebSocketsServer(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long last_tx = 0;
byte last_tx_char = 0;

// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connection from ", num);
        Serial.println(ip.toString());
      }
      break;

    // Echo text message back to client
    case WStype_TEXT:
      Serial.printf("[%u] Text: %s\n", num, payload);
      webSocket.sendTXT(num, payload);
      break;

    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}

void setup() {
  // initialize display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.invertDisplay(false);
  delay(150);
  display.clearDisplay();

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  // Start Serial port
  Serial.begin(115200);

  // Connect to access point
  Serial.println("Connecting");
  WiFi.begin(ssid, password);
  unsigned long wifi_start = millis();
  while (( WiFi.status() != WL_CONNECTED ) && (millis() - wifi_start < 5000)) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Print our IP address
    Serial.println("Connected!");
    Serial.print("My IP address: ");
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(MARGIN, MARGIN);             // Start at top-left corner
    display.println(WiFi.localIP());
    display.display();

    // Start WebSocket server and assign callback
    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
  }

  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI00);

  if (!LoRa.begin(915000000))
  {
    Serial.println("LoRa startup failed!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(MARGIN, MARGIN);
    display.println("Starting LoRa failed!");
    display.display();
    while (1);
  }
  LoRa.enableCrc();
}

void loop() {

  // Look for and handle WebSocket data
  webSocket.loop();

  if ((millis() - last_tx) > 1000) {
    digitalWrite(13, HIGH);

    // send packet
    char c = (char) ((int) 'A') + last_tx_char;
    Serial.print("LoRa: sending ");
    Serial.println(c);

    LoRa.beginPacket();
    LoRa.print(c);
    LoRa.endPacket();

    last_tx = millis();
    last_tx_char = (last_tx_char + 1) % 26;

    digitalWrite(13, LOW);
  }

  // check for LoRa packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet ");

    // read packet
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();

      String s = ((String) c) + "," + ((String) rssi) + "," + ((String) snr) + "\n";

      Serial.print(s);
      if (WiFi.status() == WL_CONNECTED) {
        webSocket.broadcastTXT(s.c_str());
      }

      display.clearDisplay();
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setTextColor(WHITE);        // Draw white text
      display.setCursor(MARGIN,MARGIN);             // Start at top-left corner
      display.println(WiFi.localIP());
      display.setCursor(SCREEN_WIDTH - (4 + MARGIN), SCREEN_HEIGHT - (8 + MARGIN));             // Start at top-left corner
      display.println(c);
      display.display();
    }
  }
}

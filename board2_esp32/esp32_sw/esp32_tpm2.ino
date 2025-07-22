
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <string.h>

#include <NeoPixelBus.h>

#include <esp_wifi.h>

#include <esp_task_wdt.h>

#define STATS_PORT 65507
#define STATS_INTERVAL 100 // ms
//IPAddress statsDestination(10, 42, 0, 1);
IPAddress statsDestination(10, 1, 20, 106);

//#define PIXEL_COUNT 1386
#define PIXEL_COUNT 800

const uint8_t PixelPin = 1; 
NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip(PIXEL_COUNT, PixelPin); //WS2812B


#define TPM2_PORT 65506
#define MAX_BUFFER_TPM2 1600 // bigger than wifi mtu, should be enough
#define TPM2_START 6
#define TPM2_HEADER_SIZE 6 // TPM2 header size in bytes

//////// STRIPE SELECTION CHANGE UNIVERSES AND ADJUST IP TO 10.0.0.<STRIPESELECT>
// adjusted via dip switches on esp32 boards
uint8_t STRIPESELECT = 0;

uint8_t tpm2Packet[MAX_BUFFER_TPM2];
uint8_t packetSize;

// TPM2 frame tracking
uint16_t lastFrameNumber = 0;
uint16_t pixelOffset = 0; // Byte offset within the pixel data

// Network Settings
//char ssid[] = "eyelan";  
//char pass[] = "";

//char ssid[] = "reborntmp";   
//char pass[] = "reborntmp"; 
// Network Settings


WiFiUDP udptpm2;
WiFiUDP udpStats;

// Global statistics counters
unsigned long droppedPackets = 0;
unsigned long discardedPackets = 0;
unsigned long invalidPackets = 0;
unsigned long displayedFrames = 0;
unsigned long processedPackets = 0;
unsigned long validPackets = 0;
unsigned long lastStatsSend = 0;


void setup()
{
  Serial.begin(115200);
    while (!Serial) {
    delay(10);
  }
  delay(500);

  Serial.println("boot");  // Read DIP switch pins to determine STRIPESELECT value
  pinMode(2, INPUT_PULLDOWN); 
  pinMode(3, INPUT_PULLDOWN);
  pinMode(4, INPUT_PULLDOWN);
  pinMode(5, INPUT_PULLDOWN);
  pinMode(6, INPUT_PULLDOWN);
  pinMode(7, INPUT_PULLDOWN);
  pinMode(8, INPUT_PULLDOWN);
  pinMode(9, INPUT_PULLDOWN);

  STRIPESELECT = 0;
  const uint8_t ip_offset = 50;
  // if (digitalRead(9) == LOW) STRIPESELECT |= (1 << 0); // not usable as pulldown on c3 supermini
  if (digitalRead(8) == LOW) STRIPESELECT |= (1 << 0);
  if (digitalRead(7) == LOW) STRIPESELECT |= (1 << 1);
  if (digitalRead(6) == LOW) STRIPESELECT |= (1 << 2);
  if (digitalRead(5) == LOW) STRIPESELECT |= (1 << 3);
  if (digitalRead(4) == LOW) STRIPESELECT |= (1 << 4);
  // if (digitalRead(3) == LOW) STRIPESELECT |= (1 << 6); // not usable as pulldown on c3 supermini
  // if (digitalRead(2) == LOW) STRIPESELECT |= (1 << 7); // not usable as pulldown on c3 supermini
  STRIPESELECT+=ip_offset;


  //delay(200*STRIPESELECT); // random Delay for connection

  //adjust to selected Stripe
  IPAddress ip(10, 0, 0, STRIPESELECT);
  IPAddress gateway(10, 0, 0, 100);
  IPAddress subnet(255, 255, 0, 0);
  //WiFi.config(ip, gateway, subnet);
  WiFi.setHostname(("esp32stripe" + String(STRIPESELECT)).c_str());
  WiFi.begin(ssid, pass);
  //WiFi.begin(ssid);
  WiFi.mode(WIFI_STA);
  Serial.print("waiting for wifi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("w");
    delay(100);
  }
  Serial.println("wifi connected");
  
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

  Serial.println(WiFi.localIP());
  
  udptpm2.begin(TPM2_PORT);
  udpStats.begin(STATS_PORT);
  strip.Begin();

  //let first pixel glow white to indicate connection
  strip.SetPixelColor(0, RgbColor (100, 100, 100));
  strip.Show();
  
  
  
  //////////// OTA Stuff
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(("esp32stripe" + String(STRIPESELECT) + "stripeselect").c_str());

  // No authentication by default
  ArduinoOTA.setPassword("admin1234(#+--)");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    WiFi.disconnect();
    delay(500);
    esp_task_wdt_init(1, true); // Reset WDT
    esp_task_wdt_add(NULL);     // Add current thread
    ESP.restart();
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  /////////////
  
}
// #define DEBUG_TPM2 1
bool data_ready = false;
void parseTPM2() {
  packetSize = udptpm2.read(tpm2Packet, MAX_BUFFER_TPM2);
  if (packetSize < (TPM2_HEADER_SIZE+1) ) {
    return; // No data received
  }

  // Increment processed packets counter
  processedPackets++;


  uint8_t startMark = tpm2Packet[0];
  uint8_t packetType = tpm2Packet[1];
  uint16_t frameSize = (tpm2Packet[2] << 8) | tpm2Packet[3];
  uint8_t frameNumber = tpm2Packet[4];
  uint8_t framesTotal = tpm2Packet[5];
#ifdef DEBUG_TPM2
  Serial.println("PKG: "+String(" sz ")+ String(packetSize) 
    + " sm "+ String(startMark) + " pt " +String(packetType) 
    + " fs "+ String(frameSize) + " fn "+ String(frameNumber) 
    + " ft "+ String(framesTotal));
#endif
  if(startMark != 0x9C){
#ifdef DEBUG_TPM2
    Serial.println("Ignore packet with wrong magic header");
#endif
    invalidPackets++;
    return;
  }
  if (packetType != 0xDA) {
#ifdef DEBUG_TPM2
    Serial.println("Discarding non data frame");
#endif
    discardedPackets++;
    return;
  }
    if (frameNumber == 1) {
#ifdef DEBUG_TPM2
      Serial.println("Frame 0");
#endif
      lastFrameNumber = 0;
      pixelOffset = 0;
    }
    else if (frameNumber < lastFrameNumber) {
#ifdef DEBUG_TPM2
    Serial.println("Discarding earlier non 0 frame");
#endif
    discardedPackets++;
    return;
    }
    else if (frameNumber > (lastFrameNumber + 1)){
#ifdef DEBUG_TPM2
    Serial.println("Detected dropped frame(s)");
#endif
    droppedPackets++;
    return;
    }

    // If we get here, the packet is valid
    validPackets++;



    lastFrameNumber = frameNumber;

    uint8_t* pixelData = strip.Pixels();

    uint16_t dataStart = TPM2_HEADER_SIZE;
    // uint16_t dataEnd = min(frameSize+TPM2_HEADER_SIZE, packetSize-1);
    uint16_t dataEnd = frameSize+TPM2_HEADER_SIZE;

    // Make sure we don't exceed the pixel data size
    uint16_t maxPixelBytes = PIXEL_COUNT * 3;
    if (pixelOffset + (dataEnd - dataStart) > maxPixelBytes) {
      dataEnd = maxPixelBytes - pixelOffset + dataStart;
    }
#ifdef DEBUG_TPM2
    Serial.println("Pixel data dims: ds" + String(dataStart) + "  de" + String(dataEnd));
#endif

    // Copy data directly to the pixel buffer
    if (dataEnd > dataStart) {
      memcpy(pixelData + pixelOffset, tpm2Packet + dataStart, dataEnd - dataStart);
      pixelOffset += (dataEnd - dataStart);
    }

    if(frameNumber==framesTotal){
      data_ready = true;
      displayedFrames++;
#ifdef DEBUG_TPM2
      Serial.println("Frame processed. Pixel offset: " + String(pixelOffset));
#endif
    }
}
int last_send = 0;
void sendStats() {
  // Create a buffer for the stats message
  char statsMessage[1000];
  snprintf(statsMessage, sizeof(statsMessage),
           "v1 dropped:%lu discarded:%lu invalid:%lu displayed:%lu processed:%lu valid:%lu\n",
           droppedPackets, discardedPackets, invalidPackets, displayedFrames, processedPackets, validPackets);

  // Send the stats via UDP
  udpStats.beginPacket(statsDestination, STATS_PORT);
  udpStats.print(statsMessage);
  udpStats.endPacket();
}

void loop()
{

  // if(data_ready && strip.CanShow()&&((millis()-last_send)>100)){
  // if(data_ready && ((millis()-last_send)>100)){
  if(data_ready && strip.CanShow()){
    last_send = millis();
    strip.Dirty();
    strip.Show(false);
    data_ready = false;
  }

  // Send stats every STATS_INTERVAL milliseconds
  if (millis() - lastStatsSend >= STATS_INTERVAL) {
    lastStatsSend = millis();
    sendStats();
  }

  while (udptpm2.parsePacket()) {
    parseTPM2();
  }
  ArduinoOTA.handle();
}

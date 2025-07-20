
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
//const uint16_t PixelCount = 300;
const uint16_t PixelCount = 10;

const uint8_t PixelPin = 1; 
NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip(PixelCount, PixelPin); //WS2812B


#define TPM2_PORT 65506
#define MAX_BUFFER_TPM2 920
#define TPM2_START 6

//////// STRIPE SELECTION CHANGE UNIVERSES AND ADJUST IP TO 10.0.0.<STRIPESELECT>
uint8_t STRIPESELECT = 0;

uint8_t tpm2Packet[MAX_BUFFER_TPM2];
uint8_t packetSize;
uint8_t* data; 

// Network Settings
//char ssid[] = "eyelan";  
//char pass[] = "";    
char ssid[] = "DEV";  
char pass[] = "DEV";    
// Network Settings


WiFiUDP udptpm2;


uint8_t net = 0;
uint8_t subnet = 0;

uint32_t unifullSize;

void setup()
{
  // Read DIP switch pins to determine STRIPESELECT value
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
  STRIPESELECT=+ip_offset;

  Serial.begin(115200);
  Serial.println("boot");
  delay(200*STRIPESELECT); // random Delay for connection

  //adjust to selected Stripe
  IPAddress ip(10, 0, 0, STRIPESELECT);
  IPAddress gateway(10, 0, 0, 100);
  IPAddress subnet(255, 255, 0, 0);
  //WiFi.config(ip, gateway, subnet);
  WiFi.setHostname(("esp32stripe" + String(STRIPESELECT)).c_str());
  WiFi.begin(ssid, pass);
  //WiFi.begin(ssid);
  WiFi.mode(WIFI_STA);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(STRIPESELECT*200 + random(10,1000));
  }
  
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);

  Serial.println(WiFi.localIP());
  
  udptpm2.begin(TPM2_PORT);
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

void sendStripeTpm2() {
           udptpm2.read(tpm2Packet, MAX_BUFFER_TPM2);
           data = tpm2Packet + TPM2_START;
             for (int i = 0; i < PixelCount; i++)
              {
                strip.SetPixelColor(i, RgbColor (data[(i * 3)], data[(i * 3 + 1)], data[(i * 3 + 2)]));  
              }   
                strip.Show();
  }
  

void loop()
{ 
  if (udptpm2.parsePacket()) { 
    sendStripeTpm2();
   }

  /////////OTA Stuff
  ArduinoOTA.handle();
  ////////
  
}

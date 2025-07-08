
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>  
#include <ArduinoOTA.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
const uint16_t PixelCount = 300;
const uint8_t PixelPin = 3; 
//NeoPixelBus<NeoGrbFeature, NeoEsp8266DmaWs2813Method> strip(PixelCount, PixelPin);
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount, PixelPin); //WS2812B

// ARTNET CODES
#define ARTNET_DATA 0x50
#define ARTNET_POLL 0x20
#define ARTNET_POLL_REPLY 0x21
#define ARTNET_PORT 6454
#define ARTNET_HEADER 17
#define MAX_BUFFER_ARTNET 360

#define TPM2_PORT 65506
#define MAX_BUFFER_TPM2 920
#define TPM2_START 6

//////// STRIPE SELECTION CHANGE UNIVERSES AND ADJUST IP TO 10.0.0.<STRIPESELECT>
#define STRIPESELECT 22

uint8_t tpm2Packet[MAX_BUFFER_TPM2];
uint8_t packetSize;
uint8_t* data; 

// Network Settings
char ssid[] = "eyelan";  
char pass[] = "";    
     
WiFiUDP udpartnet;
WiFiUDP udptpm2;

uint16_t uniSizeStripe;

//first universe
uint8_t uniData1[510]; // 510 is teilbar durch 3 ;)
uint8_t universe1 = 0;
uint16_t uniSize1;

//second universe
uint8_t uniData2[510];
uint8_t universe2 = 1;
uint16_t uniSize2;

uint8_t hData[ARTNET_HEADER + 1];
uint8_t net = 0;
uint8_t subnet = 0;

uint32_t unifullSize;

void setup()
{

  Serial.begin(115200);
  delay(200*STRIPESELECT); // random Delay for connection

  //adjust to selected Stripe
  IPAddress ip(10, 0, 0, STRIPESELECT);
  IPAddress gateway(10, 0, 0, 100);
  IPAddress subnet(255, 255, 0, 0);
  WiFi.config(ip, gateway, subnet);
  //WiFi.begin(ssid, pass);
  WiFi.begin(ssid);
  WiFi.mode(WIFI_STA);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(STRIPESELECT*200 + random(10,1000));
  }
  
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);

  // for 10 not needed
  if(STRIPESELECT == 11) {
      universe1 = 2;
      universe2 = 3;
    }

  if(STRIPESELECT == 12) {
      universe1 = 4;
      universe2 = 5;
    }

  if(STRIPESELECT == 13) {
      universe1 = 6;
      universe2 = 7;
    }

  if(STRIPESELECT == 14) {
      universe1 = 8;
      universe2 = 9;
    }

  if(STRIPESELECT == 15) {
      universe1 = 10;
      universe2 = 11;
    }

  if(STRIPESELECT == 16) {
      universe1 = 11;
      universe2 = 12;
    }
  //Serial.println(WiFi.localIP());
  
  udpartnet.begin(ARTNET_PORT);
  udptpm2.begin(TPM2_PORT);
  strip.Begin();

  //let first pixel glow white to indicate connection
  strip.SetPixelColor(0, RgbColor (100, 100, 100));  
  strip.Show();

  
  
  
  //////////// OTA Stuff
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("LEDStripe" + STRIPESELECT);

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

void   sendStripeArtnet() {

  uniSizeStripe = (unsigned short)(uniSize1 + uniSize2);

  for (int i = 0; i < PixelCount; i++)
    {   
             if(uniSize1 && (i * 3 + 2) < uniSize1) {
              //strip.SetPixelColor(i, RgbColor (255, 0, 0));
              strip.SetPixelColor(i, RgbColor (uniData1[(i * 3 + 2)], uniData1[(i * 3)], uniData1[(i * 3 + 1)]));
             }
   
             if(uniSize2 && i > 169 && (i * 3 + 2) < uniSizeStripe) {
              //strip.SetPixelColor(i, RgbColor (255, 0, 0)); // universe check light
              strip.SetPixelColor(i, RgbColor (uniData2[((i - 170) * 3 + 2)], uniData2[((i - 170) * 3)], uniData2[((i - 170) * 3 + 1)]));
             }
            
    }   
  strip.Show();
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
  if (udpartnet.parsePacket()) {
  //  digitalWrite(LED_BUILTIN,LOW);
    udpartnet.read(hData, ARTNET_HEADER + 1);
      if ( hData[8] == 0x00 && hData[9] == ARTNET_DATA && hData[15] == net ) {

        if ( hData[14] == (subnet << 4) + universe1 ) { // UNIVERSE One
          if (!uniSize1) {
            uniSize1 = (hData[16] << 8) + (hData[17]);
          }
          udpartnet.read(uniData1, uniSize1);
        }

        if ( hData[14] == (subnet << 4) + universe2 ) { // UNIVERSE two
          if (!uniSize2) {
            uniSize2 = (hData[16] << 8) + (hData[17]);
          }
          udpartnet.read(uniData2, uniSize2);
        }
          sendStripeArtnet(); 
      } // if Artnet Data
  } 
  
  if (udptpm2.parsePacket()) { 
   
    sendStripeTpm2();
   
   }
  

  /////////OTA Stuff
  ArduinoOTA.handle();
  ////////
  
}

/****************************************************************
   Copyright @ Ahan Automation
   Written By: Seyed Mohamad Mahmoodi
   Base Template for IoT Sensors
  LED States:
  1) WifiConnecting : blue blinking rapidly
  2) SensorOnly: White
  3) mqtt broker: aqua
  4) adafriut broker: green
  5) error: red
  6) warning: yellow

 ****************************************************************/
const int STATE_DELAY = 1000;
const char* OTA_NAMME = "esp_Outside_WaterTank";

//Include Files ---------------------------------------- Include Files ----------------------------------------------------------------------------
#include "secrets.h"
#include <Adafruit_NeoPixel.h>
#include <StateMachine.h> // Library by jrullan
#include <ArduinoOTA.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h> //marco Schwartz
#include "stateMachineFunctions.h"
#include "SHCSR04.h" //helder rodewrigos
SHCSR04 hcsr04;


// localMQTT client includes -------------------------- localMQTT client includes -----------------------------------------------------------------
#include <PubSubClient.h> //EspMQTTClient
extern PubSubClient localMqttClient; //mqttClient
const char* local_mqtt_server = "192.168.1.222";
long localMqttTick = 0; //lastMsg
long remoteMqttTick = 0;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// MQTT client includes -------------------------------- MQTT client includes -----------------------------------------------------------------------
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
WiFiClient              wifiClient_adaBroker_temperature;
Adafruit_MQTT_Client    adaBroker_temperature ( &wifiClient_adaBroker_temperature, AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, IO_KEY);
#include "mqttFunctions.h" // do not move it up, it depends on "pubSubClient.h"

Adafruit_MQTT_Publish saloSensTemperature = Adafruit_MQTT_Publish(&adaBroker_temperature, IO_USERNAME "/feeds/waterLevel");


// Pin Assignment ----------------------------------- Pin Assignment --------------------------------------------------------------------------------
#define rgb_LED_PIN 5

// global Variable ---------------------------------- Global Variables ------------------------------------------------------------------------------
StateMachine stMachine = StateMachine();
Adafruit_NeoPixel     pixels(1, rgb_LED_PIN, NEO_GRB + NEO_KHZ800);
int   timeElapsed = 0;
int   lastMillis = 0;
String clientId;
int noOfAdaConnectRetries = 0;
float waterLevel = 0;

// Forward Declations ------------------------------- Forward Declaration ----------------------------------------------------------------------------
void blinkRgbLed(color mcolor, int noOfRepeat = 3);
void NoConnecctionState();
void WifiConnectedState();
void LocalMqttConnectedState();
void UpdateMqttState();
void AdaAvailableState();
void UpdateAdaState();
void ReadSensorState();
bool tr_NoConnecctionWifiConnected();
bool tr_NoConnecctionReadSensor();
bool tr_WifiConnectedLocalMqttConnected();
bool tr_WifiConnectedAdaAvailable();
bool tr_LocalMqttConnectedUpdateMqtt();
bool tr_UpdateMqttAdaAvailable();
bool  tr_AdaAvailableUpdateAda();
bool tr_AdaAvailableReadSensor();
bool tr_UpdateAdaReadSensor();
bool tr_LimitNoConnecction();


// -------------------------------------------------  State Machine Control Variables  --------------------------------------------------------------
bool isWifiConnected = false;
bool isMqttConnected = false;
bool isAdaAvailable = false;
State* NoConnecction = stMachine.addState(&NoConnecctionState);
State* WifiConnected = stMachine.addState(&WifiConnectedState);
State* LocalMqttConnected = stMachine.addState(&LocalMqttConnectedState);
State* UpdateMqtt = stMachine.addState(&UpdateMqttState);
State* AdaAvailable = stMachine.addState(&AdaAvailableState);
State* UpdateAda = stMachine.addState(&UpdateAdaState);
State* ReadSensor = stMachine.addState(&ReadSensorState);



//===setup function ================================= setup functino  =================================================================================
void setup() {
  pixels.begin();
  pixels.setPixelColor(0, color::yellow);
  pixels.show();
  delay(100);

  Serial.begin(115200);
  Serial.println("BOOTING ....");
  delay(500);
  Serial.println("BOOTING ....");
  delay(500);
  Serial.println("BOOTING ....");
  delay(500);
  Serial.println();
  Serial.println("----------- OS Loaded -------------");
  Serial.println();
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  while (! Serial);


  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Hi, Configuring");

  NoConnecction->addTransition(&tr_NoConnecctionWifiConnected, WifiConnected);
  NoConnecction->addTransition(&tr_NoConnecctionReadSensor, ReadSensor);
  WifiConnected->addTransition(&tr_WifiConnectedLocalMqttConnected, LocalMqttConnected);
  WifiConnected->addTransition(&tr_WifiConnectedAdaAvailable, AdaAvailable);
  LocalMqttConnected->addTransition(&tr_LocalMqttConnectedUpdateMqtt, UpdateMqtt);
  UpdateMqtt->addTransition(&tr_UpdateMqttAdaAvailable, AdaAvailable);
  AdaAvailable->addTransition(&tr_AdaAvailableUpdateAda, UpdateAda);
  AdaAvailable->addTransition(&tr_AdaAvailableReadSensor, ReadSensor);
  UpdateAda->addTransition(&tr_UpdateAdaReadSensor, ReadSensor);
  ReadSensor->addTransition(&tr_LimitNoConnecction, NoConnecction);

  // OTA Initialization ----------------------------- OTA Initialization -----------------------------------------------------------------------------
  ArduinoOTA.setHostname(OTA_NAMME);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    Serial.println("Started updating: " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("Update complete");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int percent = progress / (total / 100);
    digitalWrite(2, (percent % 2) == 1 ? HIGH : LOW);
    Serial.printf("Progress: %u%%\n", percent);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.printf("Auth Faile\n");
    else if (error == OTA_BEGIN_ERROR)
      Serial.printf("Begin Failed\n");
    else if (error == OTA_CONNECT_ERROR)
      Serial.printf("Connect Failed\n");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.printf("Receive Failed\n");
    else if (error == OTA_END_ERROR)
      Serial.printf("End Failed\n");
  });
  ArduinoOTA.begin();
  clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  delay(100);
  randomSeed(A0);

  //MQTT Client Initialization --------------------------------------------
  localMqttClient.setServer(local_mqtt_server, 1883);
  localMqttClient.setCallback(mqttCallback);

}


//===loop function ========================== loop function  =======================================
void loop() {
  pixels.setPixelColor(0, color::black);
  pixels.show();
  //test values ////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  isWifiConnected = true;
  //  isMqttConnected = true;
  //  isAdaAvailable = false;
  //test values ////////////////////////////////////////////////////////////////////////////////////////////////////////


  //Time/Date updating ----------------------------------------------------------------
  timeElapsed = millis() - lastMillis;
  lastMillis = millis();
  remoteMqttTick += timeElapsed;
  localMqttTick += timeElapsed;


  //subsystem updated -------------------------------- subsystem updated -----------------------------------------------------------------------------
  stMachine.run();
  delay(STATE_DELAY);

  // wifi reconnect -------------------------------------- wifi reconnect ----------------------------------------------------------------------------
  if (WiFi.status() != WL_CONNECTED) {
    isWifiConnected = false;
    delay(5000);
    Serial.print("Attempting to connect to SSID: ");
    //    lcd.setCursor(0, 1);
    //    lcd.print("conning to wifi ");
    Serial.println(WLAN_SSID);
    WiFi.begin(WLAN_SSID, WLAN_PASS);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
    Serial.print(".");

    for (int i = 0; i < 25; i++) {
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      pixels.begin();
      pixels.setPixelColor(0, color::blue);
      pixels.show();
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      pixels.begin();
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
    }
  } else {
    isWifiConnected = true;
    pixels.begin();
    pixels.setPixelColor(0, color::black);
    pixels.show();
    ArduinoOTA.handle();
    localMqttClient.loop();

  }





  // read value from I/O -------------------------------------------------------




}


//=================================================================================================



void blinkRgbLed(color mcolor, int noOfRepeat) {
  // OTA Response ------------------------------------------------------
  ArduinoOTA.handle();
  for (int i = 0; i < noOfRepeat; i++)  {
    pixels.setPixelColor(0, mcolor);
    pixels.show();
    delay(100);
    pixels.setPixelColor(0, color::black);
    pixels.show();
    delay(100);
  }
}

void blinkRgbOK() {
  // OTA Response ------------------------------------------------------
  ArduinoOTA.handle();

  pixels.setPixelColor(0, color::green);
  pixels.show();
  digitalWrite(LED_BUILTIN, LOW);

  delay(1000);
  ArduinoOTA.handle();

  pixels.setPixelColor(0, color::black);
  pixels.show();
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
}

void delayOta(float delayValue) {

  ArduinoOTA.handle();
  if (delayValue > 2000) {
    pixels.setPixelColor(0, color::yellow);
    pixels.show();
    return;
  }
  delay(delayValue);
}



void reconnectLocalMqtt() {
  if (localMqttClient.connected()) {
    isMqttConnected = true;
    return;
  }
  isMqttConnected = false;

  // Attempt to connect
  Serial.println("Attempting to connect to Local MQTT Broker");
  if (localMqttClient.connect(clientId.c_str())) {
    Serial.println("local MQTT Broker is connected");
    // Subscribe
    localMqttClient.subscribe("saloon/feedingsystem/feeder01_status");
    localMqttClient.subscribe("saloon/feedingsystem/feeder02_status");
    localMqttClient.subscribe("saloon/feedingsystem/feeder03_status");
    isMqttConnected = true;

  } else {
    Serial.print("failed, rc=");
    Serial.print(localMqttClient.state());
    Serial.println(" try to connect to local again in 5 seconds");
    isMqttConnected = false;
    blinkRgbLed( aqua, 3);
  }
}


// Should be called in the loop function and it will take care if connecting.
void connect_adafruit_temperature() {
  int8_t ret;

  // Stop if already connected.
  if (adaBroker_temperature.connected()) {
    isAdaAvailable = true;
    return;
  }

  Serial.print("Connecting to Remote MQTT... ");
  isAdaAvailable = false;


  uint8_t retries = 1;
  if ((ret = adaBroker_temperature.connect()) != 0) { // connect will return 0 for connected
    Serial.println("Retrying adaBroker_temperature connection in 5 seconds...");
    adaBroker_temperature.disconnect();
    blinkRgbLed(red, 3);
    noOfAdaConnectRetries++;
    if (noOfAdaConnectRetries > 10)
      ESP.restart();
  } else {
    isAdaAvailable = true;
    noOfAdaConnectRetries = 0;
  }
}

//===================================================================================================================================================
//===================================================================================================================================================
//======================================================= state functions ===========================================================================
//===================================================================================================================================================
//===================================================================================================================================================

void NoConnecctionState() {
  Serial.println("NoConnecctionState ...");
}
bool tr_NoConnecctionWifiConnected() {
  return isWifiConnected;
  // isWifiConnected == true;
}

bool tr_NoConnecctionReadSensor() {
  return !isWifiConnected;
  //isWifiConnected = flase;

}

// ----------------------------------------------------------------------------------------------- WifiConnectedState
void WifiConnectedState() {
  Serial.println("WifiConnectedState");
  if (  localMqttTick >= (LOCAL_MQTT_UPDATE_INTERVAL * 1000)) {
    Serial.println("Time to check connection with internet services ...");
    reconnectLocalMqtt();
    connect_adafruit_temperature();
    // try to spend your time here
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = adaBroker_temperature.readSubscription(5000))) {
      //    if (subscription == &onoffbutton)
      //    {
      //      Serial.print(F("Got: "));
      //      Serial.println((char *)onoffbutton.lastread);
      //    }
    }
  }
}

bool tr_WifiConnectedLocalMqttConnected() {
  return isMqttConnected;
  //isMqttConnected == true;
}

bool tr_WifiConnectedAdaAvailable() {
  Serial.print("tr_WifiConnectedAdaAvailable() : isMqttConnected : ");
  Serial.println(isMqttConnected);
  return !isMqttConnected;
  //isMqttConnected == false;
}

// ----------------------------------------------------------------------------------------------- LocalMqttConnectedState
void LocalMqttConnectedState() {
  Serial.println("LocalMqttConnectedState");

}

bool tr_LocalMqttConnectedUpdateMqtt() {
  return true;
  //always process to next state;
}



// ----------------------------------------------------------------------------------------------- UpdateMqttState
void UpdateMqttState() {
  Serial.println("UpdateMqttState");
  pixels.begin();
  pixels.setPixelColor(0, color::aqua);
  pixels.show();
  if (  localMqttTick >= (LOCAL_MQTT_UPDATE_INTERVAL * 1000)) {
    Serial.println("Time to update local Mqtt services ...");
    localMqttTick = 0.0f;





  }
}


bool tr_UpdateMqttAdaAvailable() {
  return true;
}

// ----------------------------------------------------------------------------------------------- AdaAvailableState
void AdaAvailableState() {
  Serial.println("AdaAvailableState");
}

bool tr_AdaAvailableUpdateAda() {
  return isAdaAvailable;
  //isAdaAvailable = true;

}

bool tr_AdaAvailableReadSensor() {
  return !isAdaAvailable;
  //isAdaAvailable = false;

}

// ----------------------------------------------------------------------------------------------- UpdateAdaState
void UpdateAdaState() {
  Serial.println("UpdateAdaState");
  { //handeling connection to Adafruit lost.
    if (! adaBroker_temperature.ping()) {
      isAdaAvailable = false;
      blinkRgbLed(red, 3);
      noOfAdaConnectRetries++;
      if (noOfAdaConnectRetries > 10) {
        adaBroker_temperature.disconnect();
        ESP.restart();
      }

    } else { // Connection to Ada is Ok, Update if necessary time has passed...
      pixels.begin();   pixels.setPixelColor(0, color::green);
      pixels.show();
      noOfAdaConnectRetries = 0;
      // try to spend your time here
      if (  remoteMqttTick >= (REMOTE_MQTT_UPDATE_INTERVAL * 1000)) {
        remoteMqttTick = 0;
        Serial.println("Time to update RRRRemote ADA services ...");

        if (! saloSensTemperature.publish(waterLevel)) {
          Serial.println(F("waterLevel update Failed "));
          blinkRgbLed(yellow, 5);
        } else {
          Serial.println(F("waterLevel update OK!"));
        }

        Adafruit_MQTT_Subscribe *subscription;
        while ((subscription = adaBroker_temperature.readSubscription(5000))) {
          //    if (subscription == &onoffbutton)
          //    {
          //      Serial.print(F("Got: "));
          //      Serial.println((char *)onoffbutton.lastread);
          //    }
        }

      }
    }
  }
}

bool tr_UpdateAdaReadSensor() {
  return true;
}


// ----------------------------------------------------------------------------------------------- ReadSensorState
void ReadSensorState() {
  Serial.println("ReadSensorState");
  //  waterLevel = (330 - hcsr04.read(14, 16)  ) / 100.0f;
  //initialisation  HCSR04 (trig pin,   echo pin)
  //                       (blue,       green)
  /* socket:  1->brown
   *          2->green 
   *          3->blue
   *          4->orange
   */
  waterLevel = (hcsr04.read(14, 16)  );

  Serial.print("waterLevel: ");  Serial.println(waterLevel);
  pixels.begin();   pixels.setPixelColor(0, color::white);
  pixels.show();
}

bool tr_LimitNoConnecction() {
  return true;
}

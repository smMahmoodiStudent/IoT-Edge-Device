
WiFiClient   localWifiClient;
PubSubClient localMqttClient(localWifiClient);

void mqttCallback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message

//  "saloon/feedingsystem/feeder01_switch/set"
//  "saloon/feedingsystem/feeder02_switch/set"
//  "saloon/feedingsystem/feeder03_switch/set"
  
//  "saloon/feedingsystem/feeder01_status"
//  "saloon/feedingsystem/feeder02_status"
//  "saloon/feedingsystem/feeder03_status"
  
  
//  if (String(topic) == "test/esp32/output") {
//    Serial.println("===========================================================================");
//    Serial.print("Changing output to :");
//    if (messageTemp == "on") {
//      Serial.println("on");
//    }
//    else if (messageTemp == "off") {
//      Serial.println("off");
//    }
//  }
}

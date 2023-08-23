#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define TIME_ZONE -5

#define LDR A0
#define LED1 D1
#define LED2 D2
#define IR1 D5
#define IR2 D6
 
unsigned long lastMillis = 0;
unsigned long previousMillis = 0;
const long period = 5000;
bool light = false;
bool light2 = false;
unsigned long time_turned_on = 0;
unsigned long time_turned_on2 = 0;
int LDRValue = 0;
 
#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"
 
WiFiClientSecure net;
 
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
 
PubSubClient client(net);
 
time_t now;
time_t nowish = 1510592825;
 
 
void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
 
 
void messageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
 
 
void connectAWS()
{
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
 
  NTPConnect();
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);
 
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}
 
 
void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["LDRValue"] = LDRValue;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
 
 
void setup()
{
  Serial.begin(115200);
  
  connectAWS();

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  pinMode(LDR, INPUT);
}
 
 
void loop()
{
  LDRValue = analogRead(LDR);

  if (LDRValue > 50) {
      analogWrite(LED1, 0);
      analogWrite(LED2, 0);
      light = false;
      light2 = false;
  } else {
    if (digitalRead(IR1) == LOW && light == false) {
      light = true;
      time_turned_on = millis();
      analogWrite(LED1, 255);
    } else if (light == false) {
      analogWrite(LED1, 127);
    }

    if (light == true && millis() - time_turned_on > period) {
      light = false;
      analogWrite(LED1, 127);
    }

    if (digitalRead(IR2) == LOW && light2 == false) {
      light2 = true;
      time_turned_on2 = millis();
      analogWrite(LED2, 255);
    } else if (light == false) {
      analogWrite(LED2, 127);
    }

    if (light2 == true && millis() - time_turned_on2 > period) {
      light2 = false;
      analogWrite(LED2, 127);
    }
  }
 
  now = time(nullptr);
 
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    client.loop();
    if (millis() - lastMillis > 5000)
    {
      lastMillis = millis();
      publishMessage();
    }
  }
}
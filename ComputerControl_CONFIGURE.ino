#include <DHTesp.h>  //https://github.com/beegee-tokyo/DHTesp
#include <SimpleTimer.h>  //https://github.com/jfturcot/SimpleTimer
#include <ESP8266WiFi.h>
#include <PubSubClient.h>  //https://github.com/knolleary/pubsubclient
#include <ESP8266mDNS.h> 
#include <WiFiUdp.h>
#include <ArduinoOTA.h>  //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA

//USER CONFIGURED SECTION START//
const char* ssid = "YOUR_WIRELESS_SSID";
const char* password = "YOUR_WIRELESS_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_SERVER_ADDRESS";
const int mqtt_port = 1883;
const char *mqtt_user = "YOUR_MQTT_USERNAME";
const char *mqtt_pass = "YOUR_MQTT_PASSWORD";
const char *mqtt_client_name = "PCMCU"; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//

WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;
DHTesp dht;

// Pins

const int powerButtonPin = 14;//marked as D5 on the board
const int powerSensePin = 16;  //marked as D0 on the board



// Variables
String currentStatus = "OFF";
String oldStatus = "OFF";
char temperature[50];
char humidity[50];
bool boot = true;
bool connectWifi();

//Functions
void setup_wifi() 
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() 
{
  int retries = 0;
  while (!client.connected()) {
    if(retries < 5)
    {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass)) 
      {
        Serial.println("connected");
        if(boot == false)
        {
          client.publish("checkIn/linsdayPCMCU", "Reconnected"); 
        }
        if(boot == true)
        {
          client.publish("checkIn/PCMCU", "Rebooted");
          boot = false;
        }
        client.subscribe("commands/PC");
      } 
      else 
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        delay(5000);
      }
    }
    if(retries > 5)
    {
    ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  Serial.println(newPayload);
  Serial.println();
  if (newTopic == "commands/PC") 
  {
    if (currentStatus == "ON" && newPayload == "OFF")
    {
      digitalWrite(powerButtonPin, HIGH);
      delay(1000);
      digitalWrite(powerButtonPin, LOW);
    }
    if (currentStatus == "OFF" && newPayload == "ON")
    {
      digitalWrite(powerButtonPin, HIGH);
      delay(1000);
      digitalWrite(powerButtonPin, LOW);
    }
  }
}

void powerSense()
{
  if (pulseIn(powerSensePin, HIGH, 300000) > 100)
  {
    currentStatus = "ON";
    if(currentStatus != oldStatus)
    {
      client.publish("state/PC","ON", true);
      oldStatus = currentStatus;
    }
  }
  else if(digitalRead(powerSensePin) == HIGH)
  {
    currentStatus = "ON";
    if(currentStatus != oldStatus)
    {
      client.publish("state/PC","ON", true);
      oldStatus = currentStatus;
    }
  }
  else 
  {
    currentStatus = "OFF";
    if(currentStatus != oldStatus)
    {
      client.publish("state/PC","OFF", true);
      oldStatus = currentStatus;
    }
  }
}

void powerSenseCheckIn()
{
  if (pulseIn(powerSensePin, HIGH, 300000) > 100)
  {
      client.publish("state/PC","ON", true);
  }
  else if(digitalRead(powerSensePin) == HIGH)
  {
      client.publish("state/PC","ON", true);
  }
  else 
  {
      client.publish("state/PC","OFF", true);
  }
}


void checkIn()
{
  client.publish("checkIn/PCMCU","OK");
}


void getTemp() 
{
  float temp = dht.toFahrenheit(dht.getTemperature());
  String temp_str = String(temp);
  temp_str.toCharArray(temperature, temp_str.length() + 1);
  client.publish("guestRoom/temperature", temperature);  
  
  float humid = dht.getHumidity();
  String temp_str2 = String(humid);
  temp_str2.toCharArray(humidity, temp_str2.length() + 1);
  client.publish("guestRoom/humidity", humidity); 
}


void setup() 
{
  Serial.begin(115200);
  // Add the DHT11 sensor to pin D6 (GPIO12)
  dht.setup(12, DHTesp::DHT11);
  // GPIO Pin Setup
  pinMode(powerSensePin, INPUT_PULLDOWN_16);
  pinMode(powerButtonPin, OUTPUT);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  ArduinoOTA.setHostname("PCMCU");
  ArduinoOTA.begin(); 
  timer.setInterval(30000, getTemp);
  timer.setInterval(3000, powerSense);
  timer.setInterval(60000, powerSenseCheckIn);
  timer.setInterval(120000, checkIn);   
}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  timer.run();
}



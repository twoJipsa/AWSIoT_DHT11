#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include "DHTesp.h"

#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif 


//AWS
#include "sha256.h"
#include "Utils.h"


//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>



//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

extern "C" {
  #include "user_interface.h"
}

//AWS IOT config, change these:
char wifi_ssid[]       = "your wifi id";
char wifi_password[]   = "your wifi password";
char aws_endpoint[]    = "your aws https address";
char aws_key[]         = "your aws access key";
char aws_secret[]      = "your aws secret key";
char aws_region[]      = "your aws region";
const char* aws_topic  = "$aws/things/Thermo/shadow/update";
 
int port = 443; 

//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> client(ipstack);

#define THING_NAME "Thermo"
char clientID[] = "thermoClient";

DHTesp dht; 

char publishPayload[512];
char publishTopic[]   = "$aws/things/" THING_NAME "/shadow/update";

char *subscribeTopic[2] = {
  "$aws/things/" THING_NAME "/shadow/update/accepted", 
  "$aws/things/" THING_NAME "/shadow/get/accepted", 
};

//# of connections
long connection = 0;


//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

//count messages arrived
int arrivedcount = 0;
 
//send a message to a mqtt topic 
void dht_sendmessage(bool b_IsCallBack)
{
    char buf[200];
    
    MQTT::Message message;
    delay(dht.getMinimumSamplingPeriod());
      
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();
    
    Serial.print(dht.getStatusString());
    Serial.print("\t");
    Serial.print(humidity, 1);
    Serial.print("\t\t");
    Serial.print(temperature, 1);
    Serial.print("\t\t");
    Serial.print(dht.toFahrenheit(temperature), 1);
    Serial.print("\t\t");
    Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
    Serial.print("\t\t");
    Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);
    
    if( (b_IsCallBack == true) || (b_IsCallBack == false && temperature >= 29.0f) )
    {
      sprintf(buf, "{\"state\":{\"reported\":{\"getData\":1, \"temperature\":%lf, \"humidity\":%lf, \"heatIndex\":%lf}},\"clientToken\":\"%s\"}",
              temperature,
              humidity,
              dht.computeHeatIndex(temperature, humidity, false),
              clientID
            );
    
       message.qos = MQTT::QOS0;
       message.retained = false;
       message.dup = false;
       message.payload = (void*)buf;
       message.payloadlen = strlen(buf)+1;
       int rc = client.publish(aws_topic, message);
    }
    else
    {
      ;
    }  
}
 

unsigned long int prvTime_msec = 0;
unsigned long int sec_unit = 0;

void dht_update()
{
  unsigned long int curTime_msec;
  bool displayHumTemp = false;
  curTime_msec = millis();
  if(curTime_msec - prvTime_msec >= 1000)
  {
    prvTime_msec = curTime_msec;
    sec_unit++;
  }
  else
  {
    ;
  }

  if(sec_unit >= 300) //5분(300초)에 1회씩 온습도 정보 업데이트
  {
    sec_unit = 0;
    displayHumTemp = true;
  }
  else
  {
    ;
  }
  
  if(displayHumTemp == true)
  {
    dht_sendmessage(false); //call back이 아님
  }
  else
  {
    ;
  } 
  
} 
 
//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md)
{
  char buf[512];
  char *pch;
  int desired_command_state;
  unsigned long curTime = 0;

  MQTT::Message &message = md.message;

  Serial.print("Message ");
  Serial.print(++arrivedcount);
  Serial.print(" arrived: qos ");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  Serial.println(msg);
 
  pch = strstr(msg, "\"desired\":{\"getData\":");
  if (pch != NULL) 
  {
    pch += strlen("\"desired\":{\"getData\":");
    desired_command_state = *pch - '0'; 
    
    dht_sendmessage(true);
  }
  else
  {
    ;
  }
  delete msg;
}

//connects to websocket layer and mqtt layer
bool connect () {

    if (client.isConnected ()) {    
        client.disconnect ();
    }  
    //delay is not necessary... it just help us to get a "trustful" heap space value
    delay (1000);
    Serial.print (millis ());
    Serial.print (" - conn: ");
    Serial.print (++connection);
    Serial.print (" - (");
    Serial.print (ESP.getFreeHeap ());
    Serial.println (")");
    
   int rc = ipstack.connect(aws_endpoint, port);
    if (rc != 1)
    {
      Serial.println("error connection to the websocket server");
      return false;
    } else {
      Serial.println("websocket layer connected");
    }

    Serial.println("MQTT connecting");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 4;
    //char* clientID = generateClientID ();
    data.clientID.cstring = &clientID[0];
    rc = client.connect(data);
    //delete[] clientID;
    if (rc != 0)
    {
      Serial.print("error connection to MQTT server");
      Serial.println(rc);
      return false;
    }
    Serial.println("MQTT connected");
    return true;
}

//subscribe to a mqtt topic
void subscribe () {
   //subscript to a topic
    
    for (int i=0; i<2; i++) 
    {     
      int rc = client.subscribe(subscribeTopic[i], MQTT::QOS0, messageArrived);
      if (rc != 0) {
        Serial.print("rc from MQTT subscribe is ");
        Serial.println(rc);
        return;
      }
    }
    Serial.println("MQTT subscribed");
}


void setup() {
    wifi_set_sleep_type(NONE_SLEEP_T);
    Serial.begin (115200);
    delay (2000);
    Serial.setDebugOutput(1); 
    
    dht.setup(D7, DHTesp::DHT11); // Connect DHT sensor to GPIO 17 

    //fill with ssid and wifi password
    WiFiMulti.addAP(wifi_ssid, wifi_password);
    Serial.println ("connecting to wifi");
    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
        Serial.print (".");
    }
    Serial.println ("\nconnected");

    //fill AWS parameters    
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);
    awsWSclient.setUseSSL(true); 
 
    if (connect ()){
      subscribe ();
    }

}

void loop() {
  //keep the mqtt up and running
  if (awsWSclient.connected ()) {
      dht_update();    
      client.yield(50);
  } else {
    //handle reconnection
    if (connect ()){
      subscribe ();      
    }
  }

}

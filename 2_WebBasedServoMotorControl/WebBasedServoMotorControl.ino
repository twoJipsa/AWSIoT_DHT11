#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Servo.h>

const char* ssid = "your-wifi-id";
const char* password = "your-wifi-password";

ESP8266WebServer server(80);
Servo feedServo; 

String s = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, user-scalable=no\"></head><body><br><input type=\"button\" name=\"b1\" value=\"ON\" onclick=\"location.href='/on'\" style=\"width:100%;height:70px;font-weight:bold;font-size:1em\"><br/><input type=\"button\" name=\"b1\" value=\"OFF\" onclick=\"location.href='/off'\" style=\"width:100%;height:80px;font-weight:bold;font-size:1em\"></body></html>";

void handleRoot() { 
  server.send(200, "text/html", s); 
}

void handleNotFound(){ 
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message); 
}

void setup(void){ 
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  //on
  server.on("/on", [](){    
    feedServo.write(180);
    delay(1000); 
    feedServo.write(0);
    delay(1000); 
    Serial.println("POWER ON");
    server.send(200, "text/html", s);
  });

  //off
  server.on("/off", [](){
    feedServo.write(180);
    delay(1000); 
    feedServo.write(0);
    delay(1000); 
    Serial.println("POWER OFF");
    server.send(200, "text/html", s);
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  feedServo.attach(D5);
  feedServo.write(0);

}

void loop(void){
  server.handleClient();  
}

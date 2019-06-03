#include <ESP8266WiFi.h>
#include "FS.h"
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define Button 0
#define LED0 12
#define LED1 16
#define LED2 13
#define IRRec 14
#define IRTrans 4
#define CLIENT_TIMEOUT 5.0

const char* ssid = "YOUR WIFI SSID";
const char* password = "YOUR WIFI PASSWORD";
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 15;
const uint16_t kMinUnknownSize = 12;
String RECIRCode;
String LAYOUT = "";
bool doupdate = false;
WiFiServer server(80);
WiFiClient client;
IRsend irsend(IRTrans);
IRrecv irrecv(IRRec, kCaptureBufferSize, kTimeout, true);
decode_results results;

int LEDID = 0;

int sendpage(String filename)
{
  File f = SPIFFS.open(filename, "r");
  if (!f)
  {
      Serial.print(filename);
      Serial.println(": open failed");
      return 1;
  }
  
  String s;
  if (filename.indexOf(".jpg") != -1)
  {  //if it's a jpeg
     s = "HTTP/1.1 200 OK\r\nContent-Type: image/jpg\r\nContent-Transfer-Encoding: binary\r\n\r\n";
  }
  else if (filename.indexOf(".htm") != -1)
  {
     s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  }
  else
  {
    s= "";
  }
  
  long buf_size = 500;
  long bufi = 0;
  for (int i = 0; i < f.size(); i++)
  {
     bufi++;
     s+= (char)f.read();
     if (bufi > buf_size-1)
     {
        bufi = 0;
        client.print(s);
        s="";
     }
  }
  client.print(s);
  f.close();
  delay(1);
  return 0;
}
int wait_for_client()
{
  // Check if a client has connected
  client = server.available();
  if (!client)
  {
    delay(100);
    return 1;
  }
  long ite = CLIENT_TIMEOUT * 10;          //ite is iteration, ten per second
  while(!client.available())
  {
    delay(100);
    ite -= 1;
    if (ite < 1)
    {
      Serial.println("Client timeout");     //client connected but never became "ready."  This happens for some reason.
      client.stop();
      return 1; 
    }
  } 
  //while(1) {delay(100);}
  return 0;
}
// --------------IR --------------
bool send_ir_cmd(String cmd) //returns true if command was sent sucessfully, obviously.  accepts format "IRFORMAT-Data-datalengthinbits" example: "NEC-5132547896-32"
{     
  if (cmd.indexOf("3-") != -1)
  {   //okay, NEC, we know howo to do this
      long data = parse_long_from_string(cmd, cmd.indexOf("-")+1, cmd.lastIndexOf("-")-1);
      int datalen = parse_int_from_string(cmd, cmd.lastIndexOf("-")+1, cmd.length() -1);
      irsend.sendNEC(data, datalen);
      delay(40);
  }
  else 
  {                         // elseifs for other than NEC belong here somewhere
    return false;
  }
} 

String get_ir_cmd()
{
  RECIRCode = "";
  irrecv.setUnknownThreshold(kMinUnknownSize);
  irrecv.enableIRIn();
  
  while(true)
  {
    yield(); 
    if (irrecv.decode(&results))
    {
      if(results.bits > 0)
      {
        RECIRCode = String(results.decode_type);
        RECIRCode += "-";
        RECIRCode += uint64ToString(results.value, 10);
        RECIRCode += "-";
        RECIRCode += String(results.bits);
        Serial.println(RECIRCode);
      }
      irrecv.resume();  // Receive the next value
      break;
    }
  }

  irrecv.disableIRIn();

  return RECIRCode;
}

int parse_int_from_string(String inputstr, int startindex, int endindex) {
  int i = startindex;
  int res = 0;
  while (i <= endindex)  {
    res = res * 10 + (uint8_t)inputstr.charAt(i) - '0';
    i++;
  }
  return res;
}
long parse_long_from_string(String inputstr, int startindex, int endindex) {
  int i = startindex;
  long res = 0;
  while (i <= endindex)  {
    res = res * 10 + (uint8_t)inputstr.charAt(i) - '0';
    i++;
  }
  return res;
}

void setup()
{
  ArduinoOTA.setHostname("ESP_IR_BLASTER");
  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]()
  {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else
    { // U_SPIFFS
      type = "filesystem";
    }

    SPIFFS.end();
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]()
  {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error)
  {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
    {
      Serial.println("Auth Failed");
    } 
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
    } 
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
    } 
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
    } 
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
    }
  });
  
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  
  //IR
  irsend.begin();
  delay(10);
  //Serial
  Serial.begin(115200);
  delay(10);
  
  //WiFi
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
  server.begin();
  Serial.println("Server started");
  Serial.println(WiFi.localIP());

  //file system
  if (SPIFFS.begin())
  {
    Serial.println("File system mounted successfully");
  }
  
  Dir dir = SPIFFS.openDir("/");
  
  while (dir.next())
  {
    Serial.print(dir.fileName());
    File f = dir.openFile("r");
    Serial.print(" : ");
    Serial.println(f.size());
    f.close();
  }


   LAYOUT = ReadLayout();
}

void loop()
{
  if(doupdate)
  {
    ArduinoOTA.handle();

    if(LEDID == 0)
    {
      digitalWrite(LED0, HIGH);
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      LEDID = 1;
    }
    else
    {
      digitalWrite(LED0, LOW);
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, LOW);
      LEDID = 0;
    }
  }
  else
  {  
    if(LEDID == 0)
    {
      LEDID = 1;
      digitalWrite(LED0, HIGH);
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, LOW);
    }
    else if(LEDID == 1)
    {
      LEDID = 2;
      digitalWrite(LED0, LOW);
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, LOW);
    }
    else if(LEDID == 2)
    {
      LEDID = 0;
      digitalWrite(LED0, LOW);
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, HIGH);
    }
  }

  if (wait_for_client())
  {
    return;   //wait_for_client returns non-zero if the wait was not fruitful
  }

  // A client!
  // Read the request
  String req = client.readStringUntil(' ');
  req = client.readStringUntil(' ');
  client.flush();

  if (req.indexOf("favicon.ico") !=-1)
  {      //google chrome sends several of these annoying requests, i ignore them
    client.stop();
    return;
  }
  else if(req.length() == 1)
  {                //if the request is only a "/", then just serve the index page
     req = "/index.htm";
     sendpage(req);
  }
  else if(req.indexOf(".") == -1)
  {// if there is not "." so it's not a file in the system.. probably
    if(req == "/RECIR")
    {
      get_ir_cmd();
      String response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"IRCode\":\"" + RECIRCode + "\"}\r\n\r\n";
      client.print(response);
      client.flush();
    }
    else if(req == "/LAYOUT")
    { 
      String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + LAYOUT + "\r\n\r\n";
      client.print(response);
      client.flush();
    }
    else if(req.length() > 30)
    {
      req.remove(0,1);
      WriteLayout(req);
      LAYOUT = ReadLayout();
    }
    else if(req == "/UPDATE")
    {
      client.stop();
      server.stop();
      ArduinoOTA.begin();
      doupdate = true;
    }
    else
    {
      if (!send_ir_cmd(req))
      {
         Serial.println("Not recognized");
         Serial.println(req);
      }
    }
  }
  else
  {
     if (sendpage(req))
     {    //if page not found
        req = "/not_found.htm";
        sendpage(req);
     }
  }
}

String ReadLayout()
{
  File f = SPIFFS.open("/layout.txt", "r");
  String result = "";
  
  if (!f)
  {
      return result;
  }

  for (int i = 0; i < f.size(); i++)
  {
     result += (char)f.read();
  }
 
  f.close();

  Serial.println(result);
  
  return result;
}

void WriteLayout(String layout)
{
  File file = SPIFFS.open("/layout.txt", "w");

  if (!file)
  {
      return;
  }

  Serial.println(layout);

  file.print(layout);

  file.close();
}

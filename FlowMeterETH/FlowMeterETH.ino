// Ethernet water flow sensor
// Author: Roman Pavlyuk <roman.pavlyuk@gmail.com>
// Src: https://github.com/rpavlyuk/FlowMeter_firmware_ARD

// Needed:
// - Arduino Uno (verified) or Arduino Nano (test in progress)
// - ENC28J60 ehternet adapter
// - 3-pin flow sensor (aka "fan in the pipe")
// - External power (to use autonomously)

// PINs:
//  * ENC28J60:
//     VCC -   3.3V
//     GND -    GND
//     SCK - Pin D13
//     SO  - Pin D12 
//     SI  - Pin D11
//     CS  - Pin D8
//  * FLOW SENSOR:
//     GND (black)    - GND
//     VCC (red)      - 5V
//     DATA (yellow)  - Pin D2 

#include <UIPEthernet.h>

/*
#include <enc28j60.h>
#include <EtherCard.h>
#include <net.h>
*/

// #include <PubSubClient.h>


#include <avr/pgmspace.h>

//  #include "HttpRequest.h"

/*** BEGIN Ethernet Setup  ***/

// we have an option to use static IP addressing
// but DHCP is preferred
#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)

#if STATIC
// ethernet interface ip address
static byte myip[] = { 192,168,3,216 };
// gateway ip address
static byte gwip[] = { 192,168,1,1 };
// dns ip address
static byte dnsip[] = { 192,168,1,5 };
// mask ip
static byte maskip[] = { 255,255,0,0 };
#endif

// CS PIN
#define PIN_CS 8

// ethernet mac address - must be unique on your network
static byte mymac[6] = { 0x74,0x69,0x69,0x2D,0x35,0x32 };
// char macstr[] = "7469692d3532";
// byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
// BufferFiller bfill;

EthernetServer server = EthernetServer(80);;

/*** END Ethernet Setup  ***/

/*** START Flow sensor setup ***/

byte statusLed    = 7;

byte sensorInterrupt = 0;  // 0 = digital pin 2
byte sensorPin       = 2;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
float calibrationFactor = 4.5;

volatile unsigned long pulseCount = 0;  

const unsigned long maxPulsesToReset = 256000;

/*** END Flow sensor setup ***/

/*** BEGIN MQTT settings ***/

char servername[]="192.168.1.7";
// String clientName = String("waterflowmeter:") + macstr;
// String topicName = String("home/utility/water/inbound/json");

// PubSubClient client(servername, 1883, 0, ether);

/*** END MQTT settings ***/

/*** BEGIN other settings ***/

// Timestamp string size
#define TS_SIZE 21

// Sensor information
const char sensorId[] = "1";

// timestamp format: YYYYMMDDhhmmsskkkkkk
// kkkkkk -- microseconds as returned by datetime object in Python
char hd_val_PreviousTS[TS_SIZE] = "00000000000000000000";
char hd_val_CurrentTS[TS_SIZE] = "00000000000000000000";
const char hd_key_Received[] = "previousTS";
const char hd_key_Current[] = "currentTS";

#define USE_DEBUG   1   // set to 1 to enable debug mode
#define USE_SERIAL  0   // set to 1 to show incoming requests on serial port

//Create an object to handle the HTTP request
// HttpRequest httpReq;

/*** END other settings ***/



/*
 * Setup Network
 */
void setupNetwork() {

   // network setup
   Serial.println(F("\nInitializing Ethernet module..."));
   Ethernet.init(PIN_CS);
#if STATIC
   Ethernet.begin(mymac, myip, dnsip, gwip, maskip);  
#else
   Ethernet.begin(mymac);
#endif

   // delay(500);

   Serial.print(F("Link status: "));
   Serial.println(Ethernet.linkStatus());


   Serial.println(F("= Connection info ="));
   Serial.print(F("IP Address: "));
   Serial.println(Ethernet.localIP());
   /*
   Serial.print("Subnet Mask: ");   
   Serial.println(Ethernet.subnetMask());
   Serial.print("Gateway: ");  
   Serial.println(Ethernet.gatewayIP());
   Serial.print("DNS: ");
   Serial.println(Ethernet.dnsServerIP());
   */

   Serial.println("");
   Serial.println(F("Starting HTTP server ..."));
   server.begin();
  
}

/*
 * Setup water flow sensor
 */
void setupFlowSensor() {

  // Set up the status LED line as an output
  pinMode(statusLed, OUTPUT);
  digitalWrite(statusLed, HIGH);  // We have an active-low LED attached
  
  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);

  pulseCount        = 0;

  // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
  // Configured to trigger on a FALLING state change (transition from HIGH
  // state to LOW state)
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);  
}

/*
Insterrupt Service Routine
 */
void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;

#if USE_DEBUG
  Serial.print("Pulse count: ");
  Serial.println(pulseCount);
#endif

 /* Protect against buffer overload */
 if (pulseCount > maxPulsesToReset) {
 #if USE_DEBUG
   Serial.print(F("Resetting pulse counter!"));
 #endif
   pulseCount = 0;
 }
}

/*
 * Setup Core Routine
 */
void setup()
{
    Serial.begin(115200);

    // setup network
    setupNetwork();

    // setup flow sensor
    setupFlowSensor();
}

/*
 * Build JSON for MQTT
 * 
 */
 char* buildJson() {

   char ticks[10];
   ultoa(pulseCount, ticks, 10);
  
   // 10 chars is enough to represent unsigned long value
   char mi[10];
   ultoa(millis(), mi, 10);

   char * data;

   /*
   const static char jsonTmpl[] PROGMEM = "{"
      "\"data\" : {\n"
        "\"sensorId\" : \"%s\",\n"
        "\"sensorName\" : \"%s\",\n"
        "\"sensorDescr\" : \"%s\",\n"
        "\"ticks\" : \"%s\",\n"
        "\"currentTS\" : \"%s\",\n"
        "\"previousTS\" : \"%s\",\n"
        "\"millis\" : \"%s\"\n"
      "},\n"
      "\"status\" : \"OK\",\n"
      "\"error\" : \"N/A\"\n"
    "}\n";
    */
    sprintf(data, 
      "{"
        "\"data\" : {\n"
          "\"sensorDescr\" : \"%s\",\n"
          "\"ticks\" : \"%s\",\n"
          "\"currentTS\" : \"%s\",\n"
          "\"previousTS\" : \"%s\",\n"
          "\"millis\" : \"%s\"\n"
        "},\n"
        "\"status\" : \"OK\",\n"
        "\"error\" : \"N/A\"\n"
      "}\n", 
      sensorId,
      ticks,
      hd_val_CurrentTS,
      hd_val_PreviousTS,
      mi
      );
 
#if USE_DEBUG
   Serial.print(F("Built JSON: "));
   Serial.println(data);
   Serial.println(strlen(data));
#endif   
    return data;
}



/*
 * Build JSON object as a response to the HTTP request
 */
static char* statusHomePage() {

  char * response;

  sprintf(
    response, 
    PSTR(
      "HTTP/1.0 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "%s"
      ), 
    buildJson()
    );

}

void loop()
{
  // temporary buffer for the value of current timestamp
  char hd_val_TempTS[TS_SIZE];


  /*
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  if (pos)  // check if valid tcp data is received
  {
#if USE_DEBUG
      Serial.println("Valid TCP data received!");
#endif
      bfill = ether.tcpOffset();
      char* data = (char *) Ethernet::buffer + pos;
#if USE_SERIAL
      Serial.println(data);
#endif
      char * c = NULL;
      httpReq.resetRequest();
      for(c = data; *c != '\0'; c++) {
          httpReq.parseRequest(*c);
        
      }     
      int p_pos=httpReq.getParam((char *) hd_key_Current,hd_val_TempTS);
      if(p_pos>0){
#if USE_DEBUG
            Serial.print("DEBUG: Found current timestamp. Value: ");
            Serial.println(hd_val_TempTS);  
#endif
            // set prev timestamp to existing (saved) current timestamp (aka buffer shift: currTS -> prevTS)
            strncpy(hd_val_PreviousTS, hd_val_CurrentTS, TS_SIZE);
            
            // set 
            strncpy(hd_val_CurrentTS, hd_val_TempTS, TS_SIZE);
      }
      else {
#if USE_DEBUG
            Serial.println("DEBUG: No current timestamp parameter provided");
#endif          
      }
    
    ether.httpServerReply(statusHomePage()); // send web page data
  }

  */

  EthernetClient client = server.available();
  delay(500);
  
  if (client)
  { 
#if USE_DEBUG
    Serial.println(F("-> New Connection"));
#endif  
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
 
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
 
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {

          /*
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          //for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
          //  int sensorReading = analogRead(analogChannel);
            client.print("M7A ");
            // client.print(VA);
            client.println("<br />");
            client.print("M7B ");
//            client.print(VB);
            client.println("<br />");
          //}
          client.println("</html>");
          */

          client.println(statusHomePage());
          
          break;
          
          break;
        }
 
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r')
        {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
 
    // give the web browser time to receive the data
    delay(10);
 
    // close the connection:
    client.stop();
    Serial.println(F("   Disconnected\n"));
//    digitalWrite(rele, HIGH);
  }

}

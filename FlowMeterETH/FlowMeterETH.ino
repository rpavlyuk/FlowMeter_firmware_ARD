
// Ethernet water flow sensor
// Using:
// - ENC28J60 ehternet adapter

// PINs:
//  * ENC28J60:
//     VCC -   3.3V
//     GND -    GND
//     SCK - Pin D13
//     SO  - Pin D12 
//     SI  - Pin D11
//     CS  - Pin D8


#include <enc28j60.h>
#include <EtherCard.h>
#include <net.h>

#include <avr/pgmspace.h>

#include <HttpRequest.h>

/*** BEGIN Ethernet Setup  ***/

// we have an option to use static IP addressing
// but DHCP is preferred
#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)

#if STATIC
// ethernet interface ip address
static byte myip[] = { 192,168,1,216 };
// gateway ip address
static byte gwip[] = { 192,168,1,1 };
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x35,0x32 };

byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
BufferFiller bfill;

/*** END Ethernet Setup  ***/

/*** START Flow sensor setup ***/

byte statusLed    = 7;

byte sensorInterrupt = 0;  // 0 = digital pin 2
byte sensorPin       = 2;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
float calibrationFactor = 4.5;

volatile unsigned long pulseCount = 0;  

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

unsigned long oldTime;

/*** END Flow sensor setup ***/

/*** BEGIN other settings ***/

#define TS_SIZE 21

const char sensorId[] = "1";
const char sensorName[] = "Water Flow Sensor";
const char sensorDescr[] = "Water Flow meter for home water supply";

// timestamp format: YYYYMMDDhhmmsskkkkkk
// kkkkkk -- microseconds as returned by datetime object in Python
char hd_val_PreviousTS[TS_SIZE] = "00000000000000000000";
char hd_val_CurrentTS[TS_SIZE] = "00000000000000000000";
const char hd_key_Received[] = "previousTS";
const char hd_key_Current[] = "currentTS";

#define USE_DEBUG   1   // set to 1 to enable debug mode
#define USE_SERIAL  1   // set to 1 to show incoming requests on serial port

//Create an object to handle the HTTP request
HttpRequest httpReq;

/*** END other settings ***/



/*
 * Setup Network
 */
void setupNetwork() {

    // network setup
    Serial.println("\nInitializing Ethernet module...");
    if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
      Serial.println( "Failed to access Ethernet controller");
#if STATIC
      ether.staticSetup(myip, gwip);
#else
      if (!ether.dhcpSetup())
        Serial.println("DHCP failed");
#endif

    ether.printIp("IP:  ", ether.myip);
    ether.printIp("GW:  ", ether.gwip);  
    ether.printIp("DNS: ", ether.dnsip);   
  
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
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0;

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

  // debug
  Serial.print("Pulse count: ");
  Serial.println(pulseCount);
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

//  Here we build a web page and pass the values into it in JSON format
static word homePage() {
  
  
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "{\n"
      "\"sensorId\" : \"$S\",\n"
      "\"sensorName\" : \"$S\",\n"
      "\"sensorDescr\" : \"$S\",\n"
      "\"ticks\" : \"$S\",\n"
      "\"flowrate\" : \"$S\",\n"
      "\"flowML\" : \"$S\",\n"
      "\"flowTotalML\" : \"$S\",\n"
    "}\n"),
  sensorId, sensorName, sensorDescr);
  return bfill.position();
}

/*
 * Build JSON object as a response to the HTTP request
 */
static word statusHomePage() {
  
 char ticks[10];
 ltoa(pulseCount, ticks, 10);

 // 10 chars is enough to represent unsigned long value
 char mi[10];
 ltoa(millis(), mi, 10);
  
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "{\n"
      "\"sensorId\" : \"$S\",\n"
      "\"sensorName\" : \"$S\",\n"
      "\"sensorDescr\" : \"$S\"\n"
      "\"ticks\" : \"$S\",\n"
      "\"currentTS\" : \"$S\",\n"
      "\"previousTS\" : \"$S\",\n"
      "\"millis\" : \"$S\",\n"
    "}\n"),
  sensorId, sensorName, sensorDescr, ticks, hd_val_CurrentTS, hd_val_PreviousTS, mi);
  return bfill.position();
}

void loop()
{
  // temporary buffer for the value of current timestamp
  char hd_val_TempTS[TS_SIZE];
  
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

}


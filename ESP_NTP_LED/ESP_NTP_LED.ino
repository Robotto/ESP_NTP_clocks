//  ---- HCMS_TIME_DISPLAY.ino -----
//  By Tim Engel
//  https://github.com/Veticus
//
//  Expanded to use Paul's timeLib by Robotto
//
//  Setup of this demo:
//  Pin purpose     DevPin      Pin on Display
//
//  dataPin         - D0    - HCMS-2963 PIN4
//  registerSelect   -D1    - HCMS-2963 PIN5
//  clockPin         -D2    - HCMS-2963 PIN6
//  enable           -D3    - HCMS-2963 PIN7
//  reset            -D4    - HCMS-2963 PIN12
//  Connect pins 8 and 9 to GND
//  Connect pins 3, 10 and 11 to VCC


//  LedDisplay method takes 5 pins and an integer.
//  The last integer is the number of chars.
//  The other pins are as indicated here - in the order indicated.
//
//  The font is defined in the font5x7.h file

/*
 * TimeNTP_ESP8266WiFi.ino
 * Example showing time sync to NTP time source
 *
 * This sketch uses the ESP8266WiFi library
 */

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "LedDisplay.h"

LedDisplay myDisplay = LedDisplay(D0, D1, D2, D3, D4, 8);

#define LDR_PIN A0
#define filter_alpha 512
int ldr_val = 512;

const char ssid[] = "nope";                      //  your network SSID (name)
const char pass[] = "doubleNope";       // your network password

// NTP Servers:
static const char ntpServerName[] = "dk.pool.ntp.org";

const int timeZone = 1;     // Central European Time

WiFiUDP Udp;
unsigned int localPort = 9002;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

void setup()
{

  for(int i=0;i<1023;i++) ldr_val = (((long)ldr_val*filter_alpha)+analogRead(LDR_PIN))/(filter_alpha+1); //low pass

  myDisplay.begin();
  myDisplay.setBrightness(1);  // Brightness of the display(s) from 0 to 15
  
  
  myDisplay.home();
  //  myDisplay.print("Hello World!");
  myDisplay.setString("        Hello World!!     :)");
  for(int loopCount=0;loopCount<myDisplay.stringLength();loopCount++){  
    myDisplay.setBrightness(loopCount);
    myDisplay.scroll(-1);
    delay(100);
  }
  myDisplay.home();
  

  //WiFiManager setup:
  WiFi.hostname("LEDntp");

  Serial.begin(9600);
  delay(250);
  Serial.println("LEDntp");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

time_t prevDisplay = 0; // when the digital clock was displayed
int previousBrighness=15;

void loop()
{

  ldr_val = (((long)ldr_val*filter_alpha)+analogRead(LDR_PIN))/(filter_alpha+1); //low pass
  int brightness = map(ldr_val,0,1008,1,15); //scale ADC->LED display brightness 

  //threshold for change in brightness, so as not to write to the LEDmatrix constantly:
  if(abs(brightness-previousBrighness)>0) {
    previousBrighness=brightness;
    myDisplay.setBrightness(brightness);
  } 
  
  /*
  if(millis()==tick+200){ myDisplay.setCursor(2); myDisplay.write('.'); delay(1);} //.
  if(millis()==tick+400){ myDisplay.setCursor(5); myDisplay.write('.'); delay(1);} //.  .
  if(millis()==tick+600){ myDisplay.setCursor(2); myDisplay.write(':'); delay(1);} //.  :
  if(millis()==tick+800){ myDisplay.setCursor(5); myDisplay.write(':'); delay(1);} //:  :
  */

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if 1time has changed
      prevDisplay = now();

      Serial.print("ldr_val: "); Serial.print(ldr_val); Serial.print(' ');  Serial.print("brightness: "); Serial.print(brightness); Serial.println(' '); 
      digitalClockDisplay();
    }
  }
}

void digitalClockDisplay()
{
  myDisplay.home();           // Sets the displays cursor to the first position.
 
  // digital clock display of the time
  int hh=hour();
  if(hh<10) myDisplay.print('0');
  myDisplay.print(hh);
  
  myDisplay.print(':');

  int mm=minute();
  if(mm<10) myDisplay.print('0');
  myDisplay.print(mm);

  myDisplay.print(':');

  int ss=second();
  if(ss<10) myDisplay.print('0');
  myDisplay.print(ss);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 3000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      uint32_t epochUTC=secsSince1900 - 2208988800UL;
      return epochUTC + timeZone * SECS_PER_HOUR + getDst(epochUTC);
      //return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

/*
 * Edited from avr-libc/include/util/eu_dst.h
 * (c)2012 Michael Duane Rice All rights reserved.
*/

//DST goes true on the last sunday of march @ 2:00 (CET)
//DST goes false on the last sunday of october @ 3:00 (CEST)
 
#define MARCH 3
#define OCTOBER 10
#define SHIFTHOUR 2 

int getDst(time_t epoch) { //eats local time (CET/CEST)

  uint8_t mon, mday, hh, day_of_week, d;
  int n;

  mon = month(epoch);
  day_of_week = weekday(epoch)-1; //paul's library sets sunday==1, this code expects "days since sunday" http://www.cplusplus.com/reference/ctime/tm/
  mday = day(epoch)-1; //zero index the day as well
  hh = hour(epoch);

  if ((mon > MARCH) && (mon < OCTOBER)) return 3600;
  if (mon < MARCH) return 0;
  if (mon > OCTOBER) return 0;

  //determine mday of last Sunday 
  n = mday;
  n -= day_of_week;
  n += 7;
  d = n % 7;  // date of first Sunday
  if(d==0) d=7; //if the month starts on a monday, the first sunday is on the seventh.

  n = 31 - d;
  n /= 7; //number of Sundays left in the month 

  d = d + 7 * n;  // mday of final Sunday 

  //If the 1st of the month is a thursday, the last sunday will be on the 25th.
  //Apparently this algorithm calculates it to be on the 32nd...
  //Dirty fix, until something smoother comes along:
  if(d==31) d=24;

  if (mon == MARCH) {
    if (d > mday) return 0;
    if (d < mday) return 3600;
    if (hh < SHIFTHOUR) return 0;
    return 3600;
  }
  //the month is october:
  if (d > mday) return 3600; 
  if (d < mday) return 0; 
  if (hh < SHIFTHOUR+1) return 3600;
  return 0;
}
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

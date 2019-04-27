#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define loadPin  D2
#define clkPin   D1
#define dinPin   D0
#define blankPin D5


const char ssid[] = "nope";                      //  your network SSID (name)
const char pass[] = "suchNope";       // your network password

// NTP Servers:
static const char ntpServerName[] = "dk.pool.ntp.org";

const int timeZone = 1;     // Central European Time

WiFiUDP Udp;
unsigned int localPort = 9001;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

void setup()
{
  pinMode(dinPin, OUTPUT);
  pinMode(clkPin, OUTPUT);  
  pinMode(loadPin, OUTPUT);
  pinMode(blankPin, OUTPUT);
  digitalWrite(blankPin, LOW);

  WiFi.hostname("NTPVFD");

  Serial.begin(9600);
  delay(250);
  Serial.println("NTPVFD");
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
bool colon=false;

void loop()
{
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      colon=!colon;
      prevDisplay = now();
    }
  }
printTime(hour(),minute());
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
#define MARCH 3
#define OCTOBER 10
#define SHIFTHOUR 1 //UTC!

int getDst(time_t epoch){ //returns DST offset in seconds
  
  //unsigned long epoch = this->getEpochTime();

        uint8_t         mon, mday, hh, day_of_week, d;
        int             n;
                        //DST goes true on the last sunday of march @ 1:00 UTC (2AM CET)
                        //DST goes false on the last sunday of october @ 1:00 UTC (3AM CET)
                        mon = month(epoch);
                        day_of_week = weekday(epoch)-1; //paul's library sets sunday==1, this code expects "days since sunday" http://www.cplusplus.com/reference/ctime/tm/
                        mday = day(epoch) - 1;
                        hh = hour(epoch);
                       //DEBUG:
                      uint8_t mm=minute(epoch);
                      uint8_t ss=second(epoch);

        if              ((mon > MARCH) && (mon < OCTOBER)) return 3600;
        if              (mon < MARCH) return 0;
        if              (mon > OCTOBER) return 0;
        /* determine mday of last Sunday */
                        n = day(epoch) - 1;
                        n -= day_of_week;
                        n += 7;
                        d = n % 7;  /* date of first Sunday */
                        n = 31 - d;
                        n /= 7; /* number of Sundays left in the month */
                        d = d + 7 * n;  /* mday of final Sunday */
        if  (mon == MARCH) {
            if (d > mday) return 0;
            if (d < mday) return 3600;
            if (hh < SHIFTHOUR) return 0;
            return 3600;
            }
        //the month is october:
        if              (d > mday) return 3600;
        if              (d < mday) return 0;
        if              (hh < SHIFTHOUR) return 3600;
 
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


void printTime(int hh,int mm){
if(hh>9) numberOut(hh/10,1); //integers don't have commas, so any remainder is dropped.
else numberOut(0,1);
delay(12);
numberOut(hh%10,2);
delay(1);
if(mm>9) numberOut(mm/10,3);
else numberOut(0,3);
delay(1);
numberOut(mm%10,4);
delay(6);

}

void numberOut(int hex,int grid)
{
static unsigned int numberOutRunCounter;
numberOutRunCounter++;
const int grid_array[4] = {7,8,9,10}; //ignore the colon grid, just number the numbers 1-4
    
const int num_array[0x10][7] = {  
                          { 1,1,1,1,1,1,0 },    // 0
                          { 0,1,1,0,0,0,0 },    // 1
                          { 1,1,0,1,1,0,1 },    // 2
                          { 1,1,1,1,0,0,1 },    // 3
                          { 0,1,1,0,0,1,1 },    // 4
                          { 1,0,1,1,0,1,1 },    // 5
                          { 1,0,1,1,1,1,1 },    // 6
                          { 1,1,1,0,0,0,0 },    // 7
                          { 1,1,1,1,1,1,1 },    // 8
                          { 1,1,1,0,0,1,1 },    // 9
                          { 1,1,1,0,1,1,1 },    // A
                          { 0,0,1,1,1,1,1 },    // b
                          { 1,0,0,1,1,1,0 },    // C 
                          { 0,1,1,1,1,0,1 },    // d
                          { 1,0,0,1,1,1,1 },    // E
                          { 1,0,0,0,1,1,1 }};   // F
  //flip colon every 500ms
  if(colon==true && numberOutRunCounter%4==0) digitalWrite(dinPin,HIGH);
  else digitalWrite(dinPin,LOW);

  digitalWrite(clkPin, HIGH); //clock in 11th bit
  digitalWrite(clkPin, LOW);
  
  for (int i = 10; i >=0; i--) //clock in bits 10 downto 0
  {
      digitalWrite(dinPin,LOW); //default to low 
      if(i==grid_array[grid-1]) digitalWrite(dinPin,HIGH);
      if(i<7) digitalWrite(dinPin,num_array[hex][i]);
      digitalWrite(clkPin, HIGH);
      digitalWrite(clkPin, LOW);
  }  
   //end of conversation
  digitalWrite(loadPin, HIGH);
  digitalWrite(loadPin, LOW);

}

#include <U8glib.h>
#include <Arduino.h>
#include <math.h>
#include <RF24.h>
#include <printf.h>
#include <SPI.h>

// setup u8g object, please remove comment from one of the following constructor calls
// IMPORTANT NOTE: The following list is incomplete. The complete list of supported 
// devices with all constructor calls is here: http://code.google.com/p/u8glib/wiki/device
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);	// I2C / TWI 
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);	// Fast I2C / TWI 
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NO_ACK);	// Display which does not send AC


#define SERIAL_BAUD 57600 // The baud rate of the serial interface
#define BPW40_PIN  3
#define PERIOD_THRESHOLD 10000 // Impulse duration after which former considered junk

#define RF_CS 9
#define RF_CSN 10
#define RF_PACKET_SIZE 16 // nRF24 packet size (used for varoius calculations and comparisons)

RF24 radio(RF_CS, RF_CSN);
const uint64_t pipes[2] = { 0xe7e7e7e7e7LL, 0xc2c2c2c2c2LL };

// byte nrfCounter = 1; // A single byte to keep track of the data being sent back and forth (used with ack payload)

void setup(void) {
  Serial.begin(SERIAL_BAUD);
  printf_begin();
  pinMode(BPW40_PIN, INPUT);
  
  //u8g.setFont(u8g_font_unifont);
  u8g.setFont(u8g_font_6x10);
  u8g.setColorIndex(1); // Instructs the display to draw with a pixel on.

  radio.begin();  
  radio.openWritingPipe(pipes[0]);
  radio.setPayloadSize(RF_PACKET_SIZE);
  radio.setChannel(60);
  radio.setDataRate(RF24_2MBPS);
  radio.printDetails();

  // Extra radio configuration that can be of use
  //radio.enableAckPayload(); // Allow optional ack payloads
  //radio.setPALevel(RF24_PA_MIN);
  //radio.openReadingPipe(1, pipes[1]);
  //radio.startListening();
  //radio.writeAckPayload(1,&nrfCounter,1); // Pre-load an ack-paylod into the FIFO buffer for pipe 1

  Serial.println("Packet format: Rate (kWh) | Consumed (kW) | Imps");
}

unsigned long impulses = 0; // Impulses captured (0 .. impsKWh]
int was = HIGH; // Previous impulse state capture
unsigned long impCap; // Millisecond on which impulse was captured
unsigned int impPeriod; // Impulse period (ms)

int impsKWh = 6400; // Impulses per kWh
//int one10thKWh = (int)(impsKWh/10); // Impulses per 1/10 kWh
int one20thKWh = (int)(impsKWh/20); // Impulses per 1/20 kWh
float impsRatePerHour = 0; // Approx kWh rate
float consumedKW = 0; // Consumed kW amount

void loop(void) {
  int current = digitalRead(BPW40_PIN);

  boolean frontRise = was == LOW && current == HIGH; // Light OFF
  boolean frontFall = was == HIGH && current == LOW; // Light back to ON

  if (frontRise) { // Light impulse off (front rise) - capturing start time
    impPeriod = millis() - impCap;

    if (impPeriod <= PERIOD_THRESHOLD) { // If impulse period will be longer than threshold we dont count such bec. it seems to be abnormal impulse
      //impulses > 0 && (impulses % one10thKWh) == 0 ? consumedKW++; : NULL ; // Determining cosumption of 1/10 of kWh
      impulses > 0 && (impulses % one20thKWh) == 0 ? consumedKW += 0.05 : NULL ; // Determining cosumption of 1/20 of kWh
      
      impulses == impsKWh ? (impulses = 0) : NULL ; // Resetting imps count if we reached imps per KWh
      impulses++;

      impsRatePerHour = (3600000.0 / (float)impPeriod) / (float)impsKWh; // 3600000 millis in 1 hour
      
      char rate[5]; // Formatted rate
      dtostrf(impsRatePerHour, 4, 2, rate);

      char consumed[9]; // Formatted consumed
      dtostrf(consumedKW, 8, 2, consumed);      

      String p = "";
      p.concat(impsRatePerHour);
      p.concat("|");
      p.concat(deblank(consumed));
      p.concat("|");
      p.concat(impulses);
      
      char nrfPacket[RF_PACKET_SIZE];

      p.toCharArray(nrfPacket, RF_PACKET_SIZE-1);

      Serial.println(nrfPacket);

      radio.stopListening();
      radio.flush_tx();
      radio.write(&nrfPacket, sizeof(nrfPacket));
    } else {
      impPeriod = 0; // Setting impulse period to 0 to avoid looonnnggg display output
    }
    
    impCap = 0; // Resetting impulse capture start time bec. we captured front fall therefore value is not actual anymore
  }

  if (frontFall) { // Light impulse back to on (front fall) - detecting impulse, capturing period
    impCap = millis();
  }

  was = current;

  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_6x10);
    
    drawImps(impulses, impsKWh);
    
    u8g.setFont(u8g_font_unifont);
    
    drawPeriod(impPeriod);
    drawRate(impsRatePerHour);
    drawConsumed(consumedKW);
  } while( u8g.nextPage() );
}

char * deblank(char *str) {
  char *out = str, *put = str;

  for(; *str != '\0'; ++str) {
    if (*str != ' ')
      *put++ = *str;
  }
  
  *put = '\0';

  return out;
}

void drawImps(unsigned long impulses, int impsPerKWh) {
  String s = "";
  s.concat(impulses);
  s.concat(" Imps (");

  char dImps[5];
  dtostrf(impsPerKWh/1000.0, 3, 1, dImps);
  
  s.concat(dImps);
  s.concat("K/kWh)");

  int l = s.length();
  
  char charBuf[l];
  s.toCharArray(charBuf, l);
  
  u8g.drawStr(3, 10, charBuf);
}

void drawPeriod(int period) {
  char charBuf[12];
  sprintf(charBuf, "T: %dms", period);
  
  u8g.drawStr(3, 23, charBuf);
}

void drawConsumed(float consumed) {
  char dCons[8]; // Formatted consumed
  char charBuf[16];
  
  dtostrf(consumed, 7, 2, dCons);
  sprintf(charBuf, "Con: %s kW", dCons);
  u8g.drawStr(3, 49, charBuf);
}

void drawRate(double rate) {
  char dRate[7]; // Display rate
  char charBuf[12];
  dtostrf(rate, 4, 2, dRate);
  sprintf(charBuf, "%s kWh", dRate);
  u8g.drawStr(3, 36, charBuf);
}


#include <U8glib.h>
#include <Arduino.h>
#include <RF24.h>
#include <printf.h>
#include <SPI.h>

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);	// Fast I2C / TWI 

#define SERIAL_BAUD 57600 // The baud rate of the serial interface
#define BPW40_PIN  2
#define PERIOD_THRESHOLD 10000 // Impulse duration after which former considered junk

#define RF_CS 9
#define RF_CSN 10
#define RF_PACKET_SIZE 16 // nRF24 packet size (used for varoius calculations and comparisons)

RF24 radio(RF_CS, RF_CSN);
const uint64_t pipes[2] = { 0xe7e7e7e7e7LL, 0xc2c2c2c2c2LL };

volatile int impsKWh = 500; // Impulses per kWh
volatile unsigned long impulses = 0; // Impulses captured (0 .. impsKWh]
volatile unsigned long impCap = 0; // Millisecond on which impulse was captured
volatile unsigned int impPeriod = 0; // Impulse period (ms)
volatile float consumedKW = 0; // Consumed kW amount
volatile int one20thKWh = (int)(impsKWh/20); // Impulses per 1/20 kWh

float impsRatePerHour = 0; // Approx kWh rate
float lastConsumedKW = 0; // Last sent consumed kW amount

void capture() {
  int state = digitalRead(BPW40_PIN);
  bool isLOW = state == LOW;

  if (isLOW && impPeriod <= PERIOD_THRESHOLD) {
    impulses++; // Increasing impulse count on detection

    if ((impulses % one20thKWh) == 0) { // Increasing consumed kWh on determined resolution
      consumedKW += .05;
    }

    ((impulses % impsKWh) == 0) ? (impulses = 0) : NULL ; // Resetting imps count to avoid overflow

    impCap = millis();
  } else {
    impPeriod = millis() - impCap; // Calculated impulse period
  }
}

void setup(void) {
  Serial.begin(SERIAL_BAUD);
  printf_begin();
  
  u8g.setFont(u8g_font_6x10);
  u8g.setColorIndex(1); // Instructs the display to draw with a pixel on.

  radio.begin();  
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1, pipes[1]);
  radio.setPayloadSize(RF_PACKET_SIZE);
  radio.setChannel(60);
  radio.setDataRate(RF24_2MBPS);
  radio.powerUp();

  radio.startListening();
  
  radio.printDetails();

  pinMode(BPW40_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BPW40_PIN), capture, CHANGE);

  Serial.println("Packet format: Rate (kWh) | Consumed (kW) | Imps");
}

void loop(void) {
  if (radio.available()) {                                // if there is data ready      
    Serial.println(radio.available());
    char recvd[RF_PACKET_SIZE];
    while (radio.available()) {                            // Dump the payloads until we've gotten everything
      radio.read(&recvd, RF_PACKET_SIZE);                  // Get the payload, and see if this was the last one.
      printf("Got payload %lu @ %lu...", recvd, millis()); // Include our time, because the ping_out millis counter is unreliable due to it sleeping
    }
  }
  
  impsRatePerHour = calculateRate(impPeriod, impsKWh); // Calculating consume rate basing on last recorded impulse period

  if (consumedKW != lastConsumedKW && ((impulses % one20thKWh) == 0)) {
    sendPacket();
  }

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

float calculateRate(int period, int impsPerKWh) {
  return period <= 1 ? 0 : (float)(3600000 / period) / (float)impsPerKWh; // 3600000 millis in 1 hour
}

void sendPacket() {
  char rate[7]; // Formatted rate
  dtostrf(impsRatePerHour, 4, 2, rate);

  char consumed[11]; // Formatted consumed
  dtostrf(consumedKW, 8, 2, consumed);
  
  char nrfPacket[RF_PACKET_SIZE];

  sprintf(nrfPacket, "%s|%s|%i", rate, deblank(consumed), impulses);

  Serial.println(nrfPacket);

  radio.stopListening();
  radio.flush_tx();
  radio.write(&nrfPacket, sizeof(nrfPacket));

  delay(100);
  
  radio.startListening();
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


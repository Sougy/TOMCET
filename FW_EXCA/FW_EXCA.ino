/* Transmitter */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <NMEAGPS.h>
#include <GPSport.h>

#define CE_PIN 9
#define CSN_PIN 10

#define SEND_RATE 1000

#if !defined( NMEAGPS_PARSE_RMC )
  #error Uncomment NMEAGPS_PARSE_RMC in NMEAGPS_cfg.h!
#endif

#if !defined( GPS_FIX_TIME )
  #error Uncomment GPS_FIX_TIME in GPSfix_cfg.h!
#endif

#if !defined( GPS_FIX_LOCATION )
  #error Uncomment GPS_FIX_LOCATION in GPSfix_cfg.h!
#endif

#if !defined( GPS_FIX_SPEED )
  #error Uncomment GPS_FIX_SPEED in GPSfix_cfg.h!
#endif

#if !defined( GPS_FIX_SATELLITES )
  #error Uncomment GPS_FIX_SATELLITES in GPSfix_cfg.h!
#endif

#ifdef NMEAGPS_INTERRUPT_PROCESSING
  #error You must NOT define NMEAGPS_INTERRUPT_PROCESSING in NMEAGPS_cfg.h!
#endif

RF24 radio(CE_PIN, CSN_PIN); // CE, CSN Pins
const uint64_t address = 0xB3B4B5B6F1LL;

int32_t longitude = 1171403240; //1171403057
int32_t latitude = -4523180; //-12918241
String myString1;
String myString2;
int i;

struct package
{
  char text1[12];
  char text2[12];
  char text3[5] = "123";
  unsigned int stat = 0;
}; typedef struct package Package;
Package data;

static NMEAGPS gps;

static void doSomeWork(const gps_fix & fix);
static void doSomeWork(const gps_fix & fix)
{
  if (fix.valid.location) {
    if (fix.dateTime.seconds < 60)
      longitude = fix.longitudeL();
    latitude = fix.latitudeL();
    myString1 = String(longitude);
    myString1.toCharArray(data.text1, sizeof(data.text1));
    myString2 = String(latitude);
    myString2.toCharArray(data.text2, sizeof(data.text2));
    radio.write(&data, sizeof(data));
    Serial.println(longitude);
    Serial.println(latitude);
  } else {
    myString1 = String(longitude);
    myString1.toCharArray(data.text1, sizeof(data.text1));
    myString2 = String(latitude);
    myString2.toCharArray(data.text2, sizeof(data.text2));
    radio.write(&data, sizeof(data));
    Serial.println('?');
  }

  if (!(PIND & (1 << PIND4))) {
    data.stat = 1;
    for (i = 0; i <= 10; i++) {
      PORTB ^= (1 << PINB0);
      myString1 = String(longitude);
      myString1.toCharArray(data.text1, sizeof(data.text1));
      myString2 = String(latitude);
      myString2.toCharArray(data.text2, sizeof(data.text2));
      radio.write(&data, sizeof(data));
      delay(1000);
    }
    PORTB &= ~(1 << PINB0);
    data.stat = 0;
    i = 0;
  }
}

static void GPSloop();
static void GPSloop()
{
  while (gps.available(gpsPort))
    doSomeWork(gps.read());
}

void setup()
{
  radio.begin();
  radio.setChannel(125);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.stopListening();
  Serial.begin(9600);
  Serial.flush();
  gpsPort.begin(9600);
  DDRD &= ~(1 << PIND4);
  PORTD |= (1 << PIND4);
  pinMode(8, OUTPUT);
}
void loop()
{
  GPSloop();
}

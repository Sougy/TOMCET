/* Received */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <NMEAGPS.h>
#include <GPSport.h>

#define CE_PIN 9
#define CSN_PIN 10

#define SNED_RATE 1000

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

RF24 radio(CE_PIN, CSN_PIN);
const uint64_t address  = 0x7878787878LL;

typedef struct
{
  int32_t Longi,Lati;
  float Alti, Speed;
  int8_t Sat;
}GPScom; GPScom VARGPS;

int32_t longitude,latitude;

struct package
{
  char text1[12];
  char text2[12];
  char text3[5];
  unsigned int stat = 0;
};typedef struct package Package;
Package data;

static NMEAGPS gps;

static void doSomeWork(const gps_fix & fix);
static void doSomeWork(const gps_fix & fix)
{
  if(fix.valid.location){
    if(fix.dateTime.seconds < 60)
      VARGPS.Longi =  
  }
}


void setup() {
  // put your setup code here, to run once:
  radio.begin();
  radio.setChannel(125);
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.startListening();
  gpsPort.begin(9600);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  
}

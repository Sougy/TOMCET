/* Received */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <NMEAGPS.h>
#include <GPSport.h>

#define CE_PIN 9
#define CSN_PIN 10
#define SEND_RATE 1000
#define PORT Serial

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
const uint64_t address[]  = {0x7878787878LL, 0xB3B4B5B6F1LL};

typedef struct
{
  int32_t Longi, Lati;
  float Alti, Speed;
  int8_t Sat;
} GPScom; GPScom VARGPS;

int32_t longitude, latitude;
unsigned long PREVRF = 0;

struct package
{
  char text1[11];
  char text2[9];
  char text3[3];
  uint8_t stat = 0;
  float LSTHM;
  float ACTHM;
}; typedef struct package Package;
Package data;

static void printL( Print & outs, int32_t degE7 );
static void printL( Print & outs, int32_t degE7 )
{
  // Extract and print negative sign
  if (degE7 < 0) {
    degE7 = -degE7;
    outs.print( '-' );
  }

  // Whole degrees
  int32_t deg = degE7 / 10000000L;
  outs.print( deg );
  outs.print( '.' );

  // Get fractional degrees
  degE7 -= deg * 10000000L;

  // Print leading zeroes, if needed
  int32_t factor = 1000000L;
  while ((degE7 < factor) && (factor > 1L)) {
    outs.print( '0' );
    factor /= 10L;
  }

  // Print fractional degrees
  outs.print( degE7 );
}

static NMEAGPS gps;

static void doSomeWork(const gps_fix & fix);
static void doSomeWork(const gps_fix & fix)
{
  if (fix.valid.location) {
    if (fix.dateTime.seconds < 60)
      VARGPS.Longi  = fix.longitudeL();
    VARGPS.Lati   = fix.latitudeL();
    VARGPS.Alti   = fix.altitude();
    /*PORT.print("Longitude: ");
      printL(PORT, VARGPS.Longi);
      PORT.println();
      PORT.print("Latitude: ");
      printL(PORT, VARGPS.Lati);
      PORT.println();
      PORT.println(String("Altitude: ") + VARGPS.Alti);*/

    if (fix.valid.satellites)
      VARGPS.Sat    = fix.satellites;
    VARGPS.Speed  = fix.speed_kph();
    /*PORT.println(String("Satelit: ") + VARGPS.Sat);
      PORT.println(String("Speed kph: ") + VARGPS.Speed);*/
  }
  else {
    VARGPS.Longi  = 0;
    VARGPS.Lati   = 0;
    VARGPS.Alti   = 0;
    VARGPS.Sat    = 0;
    VARGPS.Speed  = 0;
    /*PORT.println(String("Longitude: ") + VARGPS.Longi);
      PORT.println(String("Latitude: ") + VARGPS.Lati);
      PORT.println(String("Altitude: ") + VARGPS.Alti);
      PORT.println(String("Satelit: ") + VARGPS.Sat);
      PORT.println(String("Speed mph: ") + VARGPS.Speed);*/
  }
}

static void GPSloop();
static void GPSloop()
{
  while (gps.available(gpsPort))
    doSomeWork(gps.read());
}

void RFCOM()
{
  byte pipeNum = 0;
  if ((unsigned long)(millis() - PREVRF) > SEND_RATE) {
    if (radio.available(&pipeNum)) {
      while (radio.available(&pipeNum)) {
        radio.read(&data, sizeof(data));
        if (data.stat == 20 || data.stat == 21 || data.stat == 3 || data.stat == 4 || data.stat == 5 ) {
          printL(PORT, VARGPS.Lati);
          Serial.print("|");
          printL(PORT, VARGPS.Longi);
          Serial.print("|");
          Serial.print(String(VARGPS.Alti) + '|' + VARGPS.Speed + '|' + VARGPS.Sat + '|');
          PORT.println(String(data.text3) + '|' + data.stat + '|' + data.text1 + '|' + data.text2 + '|' +
                       (data.LSTHM) + '|' + (data.ACTHM) + '|');
          PORTB ^= (1 << PINB0);
        }
        if (data.stat == 0 || data.stat == 1) {
          printL(PORT, VARGPS.Lati);
          Serial.print("|");
          printL(PORT, VARGPS.Longi);
          Serial.print("|");
          Serial.print(String(VARGPS.Alti) + "|" + VARGPS.Speed + "|" + VARGPS.Sat + "|");
          String STRLONGI = String(data.text1);
          longitude       = STRLONGI.toInt();
          String STRLATI  = String(data.text2);
          latitude        = STRLATI.toInt();
          PORT.print(String(data.text3) + '|' + data.stat + '|');
          printL(PORT, latitude);
          Serial.print("|");
          printL(PORT, longitude);
          Serial.print("|");
          PORT.println(String(data.LSTHM) + '|' + data.ACTHM + '|');
          PORTB ^= (1 << PINB0);
        }
      }
    }
    else {
      printL(PORT, VARGPS.Lati);
      Serial.print("|");
      printL(PORT, VARGPS.Longi);
      Serial.print("|");
      Serial.println(String(VARGPS.Alti) + "|" + VARGPS.Speed + "|" + VARGPS.Sat + '|' + 0 + '|' + 0 + '|' +
                     0 + '|' + 0 + '|' + 0 + '|' + 0 + '|');
      PORTB ^= (1 << PINB0);
    }
    PREVRF = millis();
  }
}

void setup() {
  // put your setup code here, to run once:
  radio.begin();
  radio.setChannel(125);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(0, address[0]);
  radio.openReadingPipe(1, address[1]);
  radio.startListening();
  gpsPort.begin(9600);
  PORT.begin(9600);
  PORT.flush();
  DDRB |= (1 << PINB0);
}

void loop() {
  // put your main code here, to run repeatedly:
  GPSloop();
  RFCOM();
}

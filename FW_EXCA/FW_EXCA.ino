/* Transmitter */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <NMEAGPS.h>
#include <GPSport.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define ANIN 3
#define HOURDIV 3600
#define MINDIV 60
#define CE_PIN 9
#define CSN_PIN 10
#define SEND_RATE 1000

#if !defined( NMEAGPS_PARSE_RMC )
#error Uncomment NMEAGPS_PARSE_RMC in NMEAGPS_cfg.h!
#endif

//#if !defined( GPS_FIX_TIME )
//#error Uncomment GPS_FIX_TIME in GPSfix_cfg.h!
//#endif

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

LiquidCrystal_I2C lcd(0x20, 16, 2);

RF24 radio(CE_PIN, CSN_PIN); // CE, CSN Pins
const uint64_t address = 0x7878787878LL;

unsigned long PREVSEN = 0;
uint8_t SAT           = 0;
int32_t longitude     = 1171403240; //1171403057
int32_t latitude      = -4523180; //-12918241
String myString1;
String myString2;

typedef struct
{
  float VOUT      = 0.0;
  float VIN       = 0.0;
  float RREF1     = 1000000.0;
  float RREF2     = 100000.0;
  uint8_t VREF    = 2;
  int VALUE       = 0;
  bool HORNSTATE  = false;
} HORN; HORN VARHORN;

typedef struct
{
  unsigned long UPTIME = 0;
  uint8_t SEC;
  uint8_t MIN;
  uint32_t HR;
} UPCHECK; UPCHECK VARUP;

struct package
{
  char text1[10];
  char text2[9];
  char text3[4] = "126";
  uint8_t stat = 0;
  float LSTHM=0;
  float ACTHM=0;
}; typedef struct package Package;
Package data;

NMEAGPS gps;
gps_fix fix;

static void doSomeWork()
{
  if ((unsigned long)(millis() - PREVSEN) > 1000) {
    if (fix.valid.location) {
      longitude = fix.longitudeL();
      latitude = fix.latitudeL();
      myString1 = String(longitude);
      myString1.toCharArray(data.text1, sizeof(data.text1));
      myString2 = String(latitude);
      myString2.toCharArray(data.text2, sizeof(data.text2));
      radio.write(&data, sizeof(data));
      Serial.println(longitude);
      Serial.println(latitude);
      Serial.println(fix.satellites);
      SAT = fix.satellites;
      Serial.println(VARHORN.VIN);
    } else {
      myString1 = String(longitude);
      myString1.toCharArray(data.text1, sizeof(data.text1));
      myString2 = String(latitude);
      myString2.toCharArray(data.text2, sizeof(data.text2));
      radio.write(&data, sizeof(data));
      Serial.println('?');
    }

    if (VARHORN.HORNSTATE) {
      data.stat = 1;
      for (int i = 0; i <= 10; i++) {
        PORTB ^= (1 << PINB0);
        myString1 = String(longitude);
        myString1.toCharArray(data.text1, sizeof(data.text1));
        myString2 = String(latitude);
        myString2.toCharArray(data.text2, sizeof(data.text2));
        radio.write(&data, sizeof(data));
        VARUP.UPTIME++;
        delay(SEND_RATE);
      }
      data.stat = 0;
      VARHORN.HORNSTATE = false;
      PORTB &= ~(1 << PINB0);
    }
    VARUP.UPTIME++;
    PREVSEN = millis();
  }
}

static void GPSloop();
static void GPSloop()
{
  while (gps.available(gpsPort))
    fix = gps.read();
  doSomeWork();
}

void UPTIME()
{
  VARUP.SEC = VARUP.UPTIME % MINDIV;
  VARUP.MIN = (VARUP.UPTIME % HOURDIV) / MINDIV;
  VARUP.HR  = VARUP.UPTIME / HOURDIV;
  if (VARUP.SEC == 0) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
  }
  else {
    lcd.setCursor(0, 0);
    lcd.print("UPTIME");
    lcd.setCursor(0, 1);
    lcd.print(String(VARUP.HR) + ':' + VARUP.MIN + ':' + VARUP.SEC + "       " + SAT);
  }
}

void INHORN()
{
  VARHORN.VALUE = analogRead(ANIN);
  VARHORN.VOUT  = (VARHORN.VALUE * 5.0) / 1024.0;
  VARHORN.VIN   = VARHORN.VOUT / (VARHORN.RREF2 / (VARHORN.RREF1 + VARHORN.RREF2));
  if (VARHORN.VIN < 0.09) {
    VARHORN.VIN = 0.0;
  }
  if (VARHORN.VIN > VARHORN.VREF) {
    VARHORN.HORNSTATE = true;
  }
}

void setup()
{
  radio.begin();
  radio.setChannel(125);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.stopListening();
  Serial.begin(9600);
  Serial.flush();
  gpsPort.begin(9600);
  //DDRD &= ~(1 << PIND4);
  //PORTD |= (1 << PIND4);
  pinMode(ANIN, INPUT);
  pinMode(8, OUTPUT);
  lcd.begin();
  lcd.backlight();
}

void loop()
{
  GPSloop();
  UPTIME();
  INHORN();
}

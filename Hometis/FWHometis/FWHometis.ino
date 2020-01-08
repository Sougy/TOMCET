#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <Eeprom24C32_64.h>

#define ANIN 0
#define CE_PIN 9
#define CSN_PIN 10
#define SEND_RATE 1000
#define HOURDIV 3600               // HOUR DIV
#define MINDIV 60                  // MINUTE DIV
#define EEPROM_ADDRESS 0x57        // RTC EEPROM ADDR
#define DS3231_I2C_ADDRESS 0x68    // RTC ADDR
#define DAYONE 0                   // ADDR IDX History HM DAY 1
#define DAYTWO 1                   // ADDR IDX History HM DAY 2
#define DAYTHREE 2                 // ADDR IDX History HM DAY 3        
#define HMEE 3                     // ADDR IDX HM value Actual
#define HMDAY 4                    // HM PER DAY
#define FLAG 5                     // ADDR IDX FLAG
#define COUNT 35                   // Size of char value (BYTES)

static Eeprom24C32_64 eeprom(EEPROM_ADDRESS);

DateTime now;
RTC_DS3231 rtc;

LiquidCrystal_I2C lcd(0x20, 16, 2);

RF24 radio(CE_PIN, CSN_PIN);
const uint64_t address = 0x7878787878LL;
unsigned long PREVSET = 0;
unsigned long PREVSEN = 0;

typedef struct
{
  const uint8_t VREF  = 2;
  uint16_t ADV        = 0;
  uint16_t EMA_S      = 0;
  float VOUT          = 0.0;
  float VIN           = 0.0;
  float EMA_a         = 0.6;
  float RREF1         = 1000000.0;
  float RREF2         = 100000.0;
} ADCPROG; ADCPROG VARADC;

typedef struct
{
  String STRCONV;
  char ACTUALBYT[COUNT];
  char RTNVAL[COUNT]  = {0}; //outputBytes from eeprom
  char RSTVAL[COUNT]  = {0}; //Reset bytes all addr eeprom
  String CURTIME;
  String CURDATE;
  uint8_t ADDRESS[6]  = {0, 42, 82, 122, 162, 202}; //40bytes long each address
  uint8_t LTCDATE;
  uint8_t SEC;
  uint8_t MIN;
  uint8_t SEQ;
  uint32_t HR;
  unsigned long SVDHM;
  unsigned long HMPDAY;
  unsigned long LASTUNIX;
  unsigned long DELTATIME;
  unsigned long ELAPSED;
  bool LTCHM = false;
} RTCOM; RTCOM VARRTC;

typedef struct
{
  byte DAYFLAG = 0x00;
} TRANSMIT; TRANSMIT VARTRANSMIT;

typedef struct
{
  char text1[12];
  char text2[12];
  char text3[5] = "TL1";
  unsigned int stat = 0;
} Package; Package data;

typedef struct
{
  String RTCIN;
  String RTCDATA[10];
  bool RTCPARSE = false;
  int RTCIDX = 0;
  int CNTR;
} RTCSET; RTCSET VARSET;

typedef struct
{
  String DATAIN;
  String SETDATA[12];
  bool INHM   = false;
  bool START  = false;
  bool END    = false;
  uint8_t VAL = 4;
  uint8_t SEQ = 0;
  uint8_t I;
  char INBYTE;
  char MSG[20];
  byte IDX;
} SERCOM; SERCOM VARSER;

void setup() {
  // put your setup code here, to run once:
  radio.begin();
  radio.setChannel(125);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.stopListening();
  Serial.begin(9600);

  eeprom.initialize();
  rtc.begin();

  //READ HMACTUAL
  eeprom.readBytes(VARRTC.ADDRESS[HMEE], COUNT, VARRTC.RTNVAL);
  String STRHM(VARRTC.RTNVAL);
  VARRTC.SVDHM      = STRHM.toInt();
  VARRTC.RTNVAL[0]  = '\0';

  //READ HM/DAY
  eeprom.readBytes(VARRTC.ADDRESS[HMDAY], COUNT, VARRTC.RTNVAL);
  String STRHM2(VARRTC.RTNVAL);
  VARRTC.HMPDAY     = STRHM2.toInt();
  VARRTC.RTNVAL[0]  = '\0';

  //READ DAYFLAG STATE
  VARTRANSMIT.DAYFLAG = eeprom.readByte(VARRTC.ADDRESS[FLAG]);
  Serial.println(VARRTC.SVDHM);
  Serial.println(VARRTC.HMPDAY);
}

void loop() {
  // put your main code here, to run repeatedly:
  PROG();
}


/*
  =======================================================================
  =========================    SERIAL program   =========================
  =======================================================================
*/
// FLUSHING SERIAL BUFFER
void SERFLUSH(void)
{
  while (true)
  {
    delay(10);
    if (Serial.available())
    {
      while (Serial.available())
        Serial.read();
      continue;
    }
    else
      break;
  }
}

//LATCH SERIAL DATA CODE
void LTCSER()
{
  while (Serial.available() > 0)
  {
    VARSER.INBYTE = Serial.read();
    if (VARSER.INBYTE == '<')
    {
      VARSER.START     = true;
      VARSER.IDX       = 0;
      VARSER.MSG[VARSER.IDX]  = '\0';
    }
    else if (VARSER.INBYTE == '>')
    {
      VARSER.END = true;
      break;
    }
    else
    {
      if (VARSER.IDX < 20)
      {
        VARSER.MSG[VARSER.IDX] = VARSER.INBYTE;
        VARSER.IDX++;
        VARSER.MSG[VARSER.IDX] = '\0';
      }
    }
  }

  if (VARSER.START && VARSER.END)
  {
    VARSER.VAL              = atoi(VARSER.MSG);
    //Serial.println(String("VAL: ") + VARSER.VAL); //for debugging only
    VARSER.IDX              = 0;
    VARSER.MSG[VARSER.IDX]  = '\0';
    VARSER.START            = false;
    VARSER.END              = false;
  }
}

//LATCH SET HM DATA IN SEC
void PARSETHM()
{
  while (Serial.available() > 0) {
    char INCHAR = (char)Serial.read();
    VARSER.DATAIN += INCHAR;
    if (INCHAR == ']') {
      VARSER.INHM  = true;
    }
  }
  if (VARSER.INHM) {
    for (VARSER.I = 1; VARSER.I < VARSER.DATAIN.length(); VARSER.I++) {
      if (VARSER.DATAIN[VARSER.I] == '[') {
        VARSER.SETDATA[VARSER.SEQ] = "";
      }
      else if (VARSER.DATAIN[VARSER.I] == ']') {
        VARSER.SEQ++;
        VARSER.SETDATA[VARSER.SEQ] = "";
      }
      else {
        VARSER.SETDATA[VARSER.SEQ] = VARSER.SETDATA[VARSER.SEQ] + VARSER.DATAIN[VARSER.I];
      }
    }
    //B4 flushing(SETDATA[0]) need to be saved to rtc saving var
    VARRTC.STRCONV        = "";
    VARRTC.SVDHM          = 0;
    VARRTC.SVDHM          = VARSER.SETDATA[0].toInt();
    Serial.println("ERASING...");
    for (int i = 0; i < 5; i++) {
      WCONV(VARRTC.RSTVAL, VARRTC.ADDRESS[i]);
      Serial.println(String("ERASE DATA FROM ADDRESS IDX: ") + i);
    }
    eeprom.writeByte(VARRTC.ADDRESS[FLAG], 0x00); //Erase FLAG State
    Serial.println("ERASED...");
    VARRTC.STRCONV = String(VARRTC.SVDHM);
    WCONV(VARRTC.STRCONV, VARRTC.ADDRESS[HMEE]);
    VARRTC.STRCONV = String(VARRTC.SVDHM);
    WCONV(VARRTC.STRCONV, VARRTC.ADDRESS[HMDAY]);

    VARSER.SEQ            = 0;
    VARSER.SETDATA[0]     = "";
    VARSER.DATAIN         = "";
  }
}


/*
  =======================================================================
  =========================   SETTING RTC TIME  =========================
  =======================================================================
*/
void SETRTC() {
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

  while (Serial.available() > 0) {
    char INCHAR = (char)Serial.read();
    VARSET.RTCIN += INCHAR;
    if (INCHAR == ']') {
      VARSET.RTCPARSE = true;
    }
  }
  if (VARSET.RTCPARSE) {
    //Serial.print("data masuk: ");
    //Serial.print(VARSET.RTCIN);
    //Serial.print("\n");

    for (VARSET.CNTR = 1; VARSET.CNTR < VARSET.RTCIN.length(); VARSET.CNTR++) {
      if (VARSET.RTCIN[VARSET.CNTR] == '[') {
        VARSET.RTCDATA[VARSET.RTCIDX] = "";
      }
      else if ((VARSET.RTCIN[VARSET.CNTR] == ']') || (VARSET.RTCIN[VARSET.CNTR] == '|')) {
        VARSET.RTCIDX++;
        VARSET.RTCDATA[VARSET.RTCIDX] = "";
      }
      else {
        VARSET.RTCDATA[VARSET.RTCIDX] = VARSET.RTCDATA[VARSET.RTCIDX] + VARSET.RTCIN[VARSET.CNTR];
      }
    }
    second      = VARSET.RTCDATA[0].toInt();
    minute      = VARSET.RTCDATA[1].toInt();
    hour        = VARSET.RTCDATA[2].toInt();
    dayOfWeek   = VARSET.RTCDATA[3].toInt();
    dayOfMonth  = VARSET.RTCDATA[4].toInt();
    month       = VARSET.RTCDATA[5].toInt();
    year        = VARSET.RTCDATA[6].toInt();

    //Serial.println(String("Detik: ") + second + " Menit: " + minute + " jam: " +
    //hour + " Hari: " + dayOfWeek + " tanggal: " + dayOfMonth + " Bulan: " + month + " Tahun: " + year);

    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0);
    Wire.write(decToBcd(second));
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));
    Wire.write(decToBcd(dayOfWeek));
    Wire.write(decToBcd(dayOfMonth));
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year));
    Wire.endTransmission();
    VARSET.RTCIDX = 0;
    VARSET.RTCIN = "";
    //Serial.println("done");
  }
}

byte decToBcd(byte val)
{
  return ( (val / 10 * 16) + (val % 10) );
}


/*
  =======================================================================
  =========================      HM program     =========================
  =======================================================================
*/
void WCONV(String DATA, uint8_t ADDR)
{
  DATA.toCharArray(VARRTC.ACTUALBYT, sizeof(VARRTC.ACTUALBYT));
  eeprom.writeBytes(VARRTC.ADDRESS[ADDR], COUNT, VARRTC.ACTUALBYT);

  //Resetting all configurations
  DATA                = "";
  VARRTC.STRCONV      = "";
  VARRTC.ACTUALBYT[0] = '\0';
}

void HMHISTORY(uint8_t ADDR)
{
  VARRTC.SVDHM += VARRTC.DELTATIME;
  VARRTC.STRCONV = String(now.hour()) + now.minute() + now.second() + '|' + now.year() +
                   now.month() + now.day() + '|' + VARRTC.HMPDAY + VARRTC.SVDHM;
  WCONV(VARRTC.STRCONV, ADDR);

  VARRTC.HMPDAY   = VARRTC.SVDHM;
  VARRTC.STRCONV  = String(VARRTC.HMPDAY);
  WCONV(VARRTC.STRCONV, HMDAY);
}

void HMS()
{
  now = rtc.now();

  if (VARADC.VIN > VARADC.VREF) {
    if (!VARRTC.LTCHM) {
      VARRTC.ELAPSED += VARRTC.DELTATIME;
      VARRTC.SVDHM += VARRTC.ELAPSED;
      VARRTC.LASTUNIX   = 0;
      VARRTC.LASTUNIX   = now.unixtime();
      VARRTC.LTCHM      = true;
    }
    VARRTC.DELTATIME = now.unixtime() - VARRTC.LASTUNIX;
  }
  else {
    if (VARRTC.LTCHM) {
      //Saving to memory
      VARRTC.SVDHM += VARRTC.DELTATIME;
      VARRTC.STRCONV  = String(VARRTC.SVDHM);
      WCONV(VARRTC.STRCONV, HMEE);

      //Resetting all configurations
      VARRTC.ELAPSED      = 0;
      VARRTC.DELTATIME    = 0;
      VARRTC.SVDHM        = 0;

      //Read from memory
      eeprom.readBytes(VARRTC.ADDRESS[HMEE], COUNT, VARRTC.RTNVAL);
      String STRHM(VARRTC.RTNVAL);
      VARRTC.SVDHM      = STRHM.toInt();
      VARRTC.RTNVAL[0]  = '\0';
      VARRTC.LTCHM      = false;
    }
  }
  //saat nyala lg waktunya blm update (SVDHM) //closed need to test NOTE: RESET DELTATIME in 2

  if (VARRTC.LTCDATE != now.day())
  {
    VARRTC.LTCDATE  = now.day();

    //**Saving to eeprom mechanism**
    if (now.hour() == 9)
    {
      if (!(VARTRANSMIT.DAYFLAG & (1 << DAYONE))) {
        HMHISTORY(DAYONE);
        VARTRANSMIT.DAYFLAG |= (1 << DAYONE);
        Serial.println("DONE D-1");
      }

      else if (!(VARTRANSMIT.DAYFLAG & (1 << DAYTWO)))
      {
        HMHISTORY(DAYTWO);
        VARTRANSMIT.DAYFLAG |= (1 << DAYTWO);
        Serial.println("DONE D-2");
      }

      else if (!(VARTRANSMIT.DAYFLAG & (1 << DAYTHREE)))
      {
        HMHISTORY(DAYTHREE);
        VARTRANSMIT.DAYFLAG |= (1 << DAYTHREE);
        Serial.println("DONE D-3");
      }
      eeprom.writeByte(VARRTC.ADDRESS[FLAG], VARTRANSMIT.DAYFLAG);
      Serial.println("DAY SAVED");
    }
  }

  VARRTC.CURTIME  = String(now.hour()) + ":" + now.minute() + ":" + now.second();
  VARRTC.CURDATE  = String(now.year()) + "-" + now.month() + "-" + now.day();
  VARRTC.SEC      = (VARRTC.SVDHM + VARRTC.DELTATIME) % MINDIV;
  VARRTC.MIN      = ((VARRTC.SVDHM + VARRTC.DELTATIME) % HOURDIV) / MINDIV;
  VARRTC.HR       = (VARRTC.SVDHM + VARRTC.DELTATIME) / HOURDIV;
}


/*
  ============================================================================
  =========================   SENSOR STATE program   =========================
  ============================================================================
*/
void LTCSEN()
{
  HMS();

  VARADC.ADV    = analogRead(ANIN);
  VARADC.EMA_S  = (VARADC.EMA_a * VARADC.ADV) + ((1 - VARADC.EMA_a) * VARADC.EMA_S);
  VARADC.VOUT   = (VARADC.EMA_S * 5.0) / 1024.0;
  VARADC.VIN    = VARADC.VOUT / (VARADC.RREF2 / (VARADC.RREF1 + VARADC.RREF2));

  if (VARADC.VIN < 1)
  {
    VARADC.VIN = 0;
  }

  if ((unsigned long)(millis() - PREVSEN) > SEND_RATE) {
    Serial.println(String(VARRTC.HR) + '/' + VARRTC.MIN + '/' + VARRTC.SEC);
    Serial.println(String(VARRTC.CURDATE) + '/' + VARRTC.CURTIME);
    Serial.println(VARRTC.SVDHM);
    Serial.println(VARRTC.HMPDAY);
    PREVSEN = millis();
  }
}


/*
  =======================================================================
  =========================   program PROCESS   =========================
  =======================================================================
*/
void PROG()
{
  LTCSEN();
  LTCSER();

  switch (VARSER.VAL) {
    case 1:
      SERFLUSH();
      while (!VARSET.RTCPARSE) {
        if ((unsigned long)(millis() - PREVSET) > SEND_RATE) {
          Serial.println('&');
          SETRTC();
          PREVSET = millis();
        }
      }
      VARSET.RTCPARSE = false;
      VARSER.VAL      = 0;

      break;

    case 2:
      SERFLUSH();
      VARRTC.DELTATIME  = 0;
      VARRTC.ELAPSED    = 0;
      while (!VARSER.INHM) {
        if ((unsigned long)(millis() - PREVSET) > SEND_RATE) {
          Serial.println('#');
          PARSETHM();
          PREVSET = millis();
        }
      }
      VARSER.INHM = false;
      VARSER.VAL  = 0;

      break;
  }
}

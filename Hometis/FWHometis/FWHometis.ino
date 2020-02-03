#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include "RTClib.h"
#include <Eeprom24C32_64.h>
#include <LiquidCrystal_I2C.h>

#define ANIN 0
#define CE_PIN 9
#define CSN_PIN 10
#define SEND_RATE 1000
#define HOURDIV 3600                // HOUR DIV
#define MINDIV 60                   // MINUTE DIV
#define EEPROM_ADDRESS 0x57         // EEPROM ADDR
#define DS3231_I2C_ADDRESS 0x68     // RTC ADDR
#define LCDADDR 0x27
#define DAYONE 0                    // ADDR IDX History HM DAY 1
#define DAYTWO 1                    // ADDR IDX History HM DAY 2
#define DAYTHREE 2                  // ADDR IDX History HM DAY 3        
#define HMEE 3                      // ADDR IDX HM value Actual
#define HMDAY 4                     // HM PER DAY
#define FLAG 5                      // ADDR IDX FLAG
#define YSTDAY 6                    // ADDR LSTD
#define YSTIME 7                    // ADDR LSTT

LiquidCrystal_I2C lcd(LCDADDR, 16, 2);

RF24 radio(CE_PIN, CSN_PIN);
const uint64_t address  = 0xB3B4B5B6F1LL;

static Eeprom24C32_64 eeprom(EEPROM_ADDRESS);
DateTime now;
RTC_DS3231 rtc;

unsigned long PREVSET = 0;
unsigned long PREVSEN = 0;
const byte BYTLEN     = 45;
uint8_t rfInterval    = 0;

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
  String LSTD;
  String LSTT;
  String ENGSTAT = "OFF";
  String RTN;
  String STRCONV;
  String CURTIME;
  String CURDATE;
  byte DAYFLAG          = 0x00;
  byte ACTUALBYT[BYTLEN];
  byte RTNVAL[BYTLEN]   = {0}; //outputBytes from eeprom
  byte RSTVAL[BYTLEN]   = {0}; //Reset bytes all addr eeprom
  const word ADDRESS[8] = {0, 60, 120, 180, 240, 300, 360, 420}; //60bytes long each address
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
  char RFDATE[10];
  char RFTIME[9];
  char NOUNIT[4]  = "123";
  uint8_t stat    = 20;
  float LSTHM;
  float ACTHM;
} RFCOM; RFCOM VARRF;

typedef struct
{
  char RFDATE[10];
  char RFTIME[9];
  char NOUNIT[4]  = "123";
  uint8_t stat    = 3;
  float LSTHM;
  float ACTHM;
} BUFFRFCOM; BUFFRFCOM VARBUFF;

typedef struct
{
  String CONVDAT;
  char DAY[3][50];
  char SVDDATE[3][10];
  char SVDTIME[3][10];
  char SVDHMPDAY[3][10];
  char SVDHMACT[3][10];
} CONV; CONV VARCONV;

typedef struct
{
  String RTCIN;
  String RTCDATA[10];
  bool RTCPARSE = false;
  int RTCIDX    = 0;
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
  Serial.begin(9600);
  radio.begin();
  radio.setChannel(125);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.stopListening();
  lcd.begin();
  lcd.backlight();
  eeprom.initialize();
  rtc.begin();
  STPMEM();
}

void loop() {
  // put your main code here, to run repeatedly:
  PROG();
}

char* getTagValue(char* a_tag_list, char* a_tag)
{
  /* 'strtok' modifies the string. */
  char* tag_list_copy = malloc(strlen(a_tag_list) + 1);
  char* result        = 0;
  char* s;

  strcpy(tag_list_copy, a_tag_list);

  s = strtok(tag_list_copy, "&");
  while (s)
  {
    char* equals_sign = strchr(s, '=');
    if (equals_sign)
    {
      *equals_sign = 0;
      if (0 == strcmp(s, a_tag))
      {
        equals_sign++;
        result = malloc(strlen(equals_sign) + 1);
        strcpy(result, equals_sign);
      }
    }
    s = strtok(0, "&");
  }
  free(tag_list_copy);

  return result;
}

void SPLITVAL(int VAL)
{
  char* DATE;
  char* TIME;
  char* LAST;
  char* ACTH;

  eeprom.readBytes(VARRTC.ADDRESS[VAL], BYTLEN, VARRTC.RTNVAL);
  VARRTC.RTN = String((char*)VARRTC.RTNVAL);
  VARRTC.RTN.toCharArray(VARCONV.DAY[VAL], sizeof(VARCONV.DAY[VAL]));

  DATE  = getTagValue(VARCONV.DAY[VAL], "D"); VARCONV.CONVDAT = String(DATE);
  VARCONV.CONVDAT.toCharArray(VARCONV.SVDDATE[VAL], sizeof(VARCONV.SVDDATE[VAL]));

  TIME  = getTagValue(VARCONV.DAY[VAL], "T"); VARCONV.CONVDAT = String(TIME);
  VARCONV.CONVDAT.toCharArray(VARCONV.SVDTIME[VAL], sizeof(VARCONV.SVDTIME[VAL]));

  LAST  = getTagValue(VARCONV.DAY[VAL], "L"); VARCONV.CONVDAT = String(LAST);
  VARCONV.CONVDAT.toCharArray(VARCONV.SVDHMPDAY[VAL], sizeof(VARCONV.SVDHMPDAY[VAL]));

  ACTH  = getTagValue(VARCONV.DAY[VAL], "A"); VARCONV.CONVDAT = String(ACTH);
  VARCONV.CONVDAT.toCharArray(VARCONV.SVDHMACT[VAL], sizeof(VARCONV.SVDHMACT[VAL]));

  free(DATE);
  free(TIME);
  free(LAST);
  free(ACTH);
}

void STPMEM()
{
  eeprom.readBytes(VARRTC.ADDRESS[HMEE], BYTLEN, VARRTC.RTNVAL);
  VARRTC.RTN = String((char*)VARRTC.RTNVAL);
  VARRTC.SVDHM  = VARRTC.RTN.toInt();
  //Serial.println(VARRTC.SVDHM);

  eeprom.readBytes(VARRTC.ADDRESS[HMDAY], BYTLEN, VARRTC.RTNVAL);
  VARRTC.RTN = String((char*)VARRTC.RTNVAL);
  VARRTC.HMPDAY  = VARRTC.RTN.toInt();
  //Serial.println(VARRTC.HMPDAY);

  VARRTC.DAYFLAG = eeprom.readByte(VARRTC.ADDRESS[FLAG]);

  if (VARRTC.DAYFLAG & (1 << DAYONE))
  {
    SPLITVAL(DAYONE);
    if (VARRTC.DAYFLAG & (1 << DAYTWO))
    {
      SPLITVAL(DAYTWO);
      if (VARRTC.DAYFLAG & (1 << DAYTHREE))
      {
        SPLITVAL(DAYTHREE);
      }
    }
  }
  BUFFRF(DAYONE);

  eeprom.readBytes(VARRTC.ADDRESS[YSTDAY], BYTLEN, VARRTC.RTNVAL);
  VARRTC.LSTD = String((char*)VARRTC.RTNVAL);
  eeprom.readBytes(VARRTC.ADDRESS[YSTIME], BYTLEN, VARRTC.RTNVAL);
  VARRTC.LSTT = String((char*)VARRTC.RTNVAL);

  Serial.println(String("LASTDAY EON: ") + VARRTC.LSTD);
  Serial.println(String("LASTIME EON: ") + VARRTC.LSTT);
  Serial.println(String("D1: ") + VARCONV.DAY[DAYONE]);
  Serial.println(String("D2: ") + VARCONV.DAY[DAYTWO]);
  Serial.println(String("D3: ") + VARCONV.DAY[DAYTHREE]);
  //Serial.println(String("D1: ") + VARCONV.SVDDATE[DAYONE] + '|' + VARCONV.SVDTIME[DAYONE] + '|' + VARCONV.SVDHMPDAY[DAYONE] + '|' + VARCONV.SVDHMACT[DAYONE]);
  //Serial.println(String("D1: ") + VARBUFF.RFDATE + '|' + VARBUFF.RFTIME + '|' + VARBUFF.LSTHM + '|' + VARBUFF.ACTHM);
}

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

void HMSET()
{
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("ERASING");
  //Serial.println("Erasing");

  for (int i = 0; i < 6; i++) {
    eeprom.writeBytes(VARRTC.ADDRESS[i], BYTLEN, VARRTC.RSTVAL);
    Serial.println(String("Remove data from address index...") + i);
    delay(100);
  }
  VARRTC.STRCONV  = String("0");
  VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
  eeprom.writeBytes(VARRTC.ADDRESS[YSTDAY], BYTLEN, VARRTC.ACTUALBYT);
  VARRTC.STRCONV  = "";

  VARRTC.STRCONV  = String("0");
  VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
  eeprom.writeBytes(VARRTC.ADDRESS[YSTIME], BYTLEN, VARRTC.ACTUALBYT);

  eeprom.writeByte(VARRTC.ADDRESS[FLAG], 0x00);

  delay(1500);
  lcd.clear();
  //Serial.println("Erased");
  lcd.setCursor(4, 0);
  lcd.print("ERASED");

  delay(500);
  lcd.setCursor(4, 0);
  lcd.print("SAVING");

  //Serial.println(String("SVDHM: ") + VARRTC.SVDHM); //for debugging only
  VARRTC.STRCONV  = String(VARRTC.SVDHM);
  VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
  eeprom.writeBytes(VARRTC.ADDRESS[HMEE], BYTLEN, VARRTC.ACTUALBYT);
  eeprom.writeBytes(VARRTC.ADDRESS[HMDAY], BYTLEN, VARRTC.ACTUALBYT);

  delay(1500);
  VARSER.SEQ            = 0;
  VARSER.SETDATA[0]     = "";
  VARSER.DATAIN         = "";
  VARRTC.STRCONV        = "";
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
    VARRTC.SVDHM          = 0;
    VARRTC.HMPDAY         = 0;
    VARRTC.SVDHM          = VARSER.SETDATA[0].toInt();
    VARRTC.HMPDAY         = VARSER.SETDATA[0].toInt();

    HMSET();
  }
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
      VARRTC.ENGSTAT    = "ON";
      VARRF.stat        = 21;
      lcd.clear();
      if (!(VARRTC.DAYFLAG & (1 << YSTDAY))) {
        VARRTC.LSTD = String(now.day()) + '-' + now.month() + '-' + now.year();
        VARRTC.LSTT = String(now.hour()) + ':' + now.minute() + ':' + now.second();
        VARRTC.LSTD.getBytes(VARRTC.ACTUALBYT, BYTLEN);
        eeprom.writeBytes(VARRTC.ADDRESS[YSTDAY], BYTLEN, VARRTC.ACTUALBYT);
        VARRTC.LSTT.getBytes(VARRTC.ACTUALBYT, BYTLEN);
        eeprom.writeBytes(VARRTC.ADDRESS[YSTIME], BYTLEN, VARRTC.ACTUALBYT);
        eeprom.writeByte(VARRTC.ADDRESS[FLAG], VARRTC.DAYFLAG);
        VARRTC.DAYFLAG |= (1 << YSTDAY);
      }
    }
    VARRTC.DELTATIME = now.unixtime() - VARRTC.LASTUNIX;
  }
  else {
    if (VARRTC.LTCHM) {
      //Saving to memory
      VARRTC.SVDHM += VARRTC.DELTATIME;
      VARRTC.STRCONV  = String(VARRTC.SVDHM);
      VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
      eeprom.writeBytes(VARRTC.ADDRESS[HMEE], BYTLEN, VARRTC.ACTUALBYT);

      //Resetting all configurations
      VARRTC.ELAPSED      = 0;
      VARRTC.DELTATIME    = 0;
      VARRTC.SVDHM        = 0;
      VARRTC.STRCONV      = "";
      VARRTC.ENGSTAT      = "OFF";
      VARRF.stat          = 20;

      //Read from memory
      eeprom.readBytes(VARRTC.ADDRESS[HMEE], BYTLEN, VARRTC.RTNVAL);
      VARRTC.RTN = String((char*)VARRTC.RTNVAL);
      VARRTC.SVDHM  = VARRTC.RTN.toInt();
      VARRTC.LTCHM      = false;
    }
  }

  VARRTC.CURTIME  = String(now.hour()) + ":" + now.minute() + ":" + now.second();
  VARRTC.CURDATE  = String(now.day()) + "-" + now.month() + "-" + now.year();
  VARRTC.SEC      = (VARRTC.SVDHM + VARRTC.DELTATIME) % MINDIV;
  VARRTC.MIN      = ((VARRTC.SVDHM + VARRTC.DELTATIME) % HOURDIV) / MINDIV;
  VARRTC.HR       = (VARRTC.SVDHM + VARRTC.DELTATIME) / HOURDIV;
  VARRF.LSTHM    = VARRTC.HMPDAY / 3600.0;
  VARRF.ACTHM    = (VARRTC.SVDHM + VARRTC.DELTATIME) / 3600.0;
}

void HMHIS(uint8_t ADDR)
{
  unsigned long SVDHMACT = VARRTC.SVDHM + VARRTC.DELTATIME;
  String HMACTCONV  = String(SVDHMACT / 3600.0);
  String HMLSTCONV  = String(VARRTC.HMPDAY / 3600.0);
  VARRTC.STRCONV    = String("D=" + VARRTC.LSTD + "&T=" + VARRTC.LSTT
                             + "&L=" + HMLSTCONV + "&A=" + HMACTCONV);

  VARRTC.LSTD.toCharArray(VARCONV.SVDDATE[ADDR], sizeof(VARCONV.SVDDATE[ADDR]));
  VARRTC.LSTT.toCharArray(VARCONV.SVDTIME[ADDR], sizeof(VARCONV.SVDTIME[ADDR]));
  HMACTCONV.toCharArray(VARCONV.SVDHMACT[ADDR], sizeof(VARCONV.SVDHMACT[ADDR]));
  HMLSTCONV.toCharArray(VARCONV.SVDHMPDAY[ADDR], sizeof(VARCONV.SVDHMPDAY[ADDR]));

  VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
  eeprom.writeBytes(VARRTC.ADDRESS[ADDR], BYTLEN, VARRTC.ACTUALBYT);

  VARRTC.STRCONV  = "";

  VARRTC.HMPDAY   = SVDHMACT;
  VARRTC.STRCONV  = String(VARRTC.HMPDAY);
  VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
  eeprom.writeBytes(VARRTC.ADDRESS[HMDAY], BYTLEN, VARRTC.ACTUALBYT);

  VARRTC.STRCONV  = "";

  VARRTC.STRCONV  = String("0");
  VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
  eeprom.writeBytes(VARRTC.ADDRESS[YSTDAY], BYTLEN, VARRTC.ACTUALBYT);
  VARRTC.LSTD = VARRTC.STRCONV;

  VARRTC.STRCONV  = "";

  VARRTC.STRCONV  = String("0");
  VARRTC.STRCONV.getBytes(VARRTC.ACTUALBYT, BYTLEN);
  eeprom.writeBytes(VARRTC.ADDRESS[YSTIME], BYTLEN, VARRTC.ACTUALBYT);
  VARRTC.LSTT = VARRTC.STRCONV;

  VARRTC.DAYFLAG &= ~(1 << YSTDAY);
  VARRTC.DAYFLAG |= (1 << ADDR);
}

void DAYSAVE()
{
  if (VARRTC.LTCDATE != now.day())
  {
    VARRTC.LTCDATE  = now.day();
    //**Saving to eeprom mechanism**
    if (now.hour() == 0)
    {
      if (!(VARRTC.DAYFLAG & (1 << DAYONE)))
      {
        HMHIS(DAYONE);
        delay(1000);
        Serial.println("DONE D-1");
      }

      else if (!(VARRTC.DAYFLAG & (1 << DAYTWO)))
      {
        HMHIS(DAYTWO);
        delay(1000);
        Serial.println("DONE D-2");
      }

      else if (!(VARRTC.DAYFLAG & (1 << DAYTHREE)))
      {
        HMHIS(DAYTHREE);
        delay(1000);
        Serial.println("DONE D-3");
      }
      else if (VARRTC.DAYFLAG & 0x07) {
        eeprom.readBytes(VARRTC.ADDRESS[DAYTWO], BYTLEN, VARRTC.RTNVAL);
        VARRTC.RTN = String((char*)VARRTC.RTNVAL);
        VARRTC.RTN.getBytes(VARRTC.ACTUALBYT, BYTLEN);
        eeprom.writeBytes(VARRTC.ADDRESS[DAYONE], BYTLEN, VARRTC.ACTUALBYT);

        eeprom.readBytes(VARRTC.ADDRESS[DAYTHREE], BYTLEN, VARRTC.RTNVAL);
        VARRTC.RTN = String((char*)VARRTC.RTNVAL);
        VARRTC.RTN.getBytes(VARRTC.ACTUALBYT, BYTLEN);
        eeprom.writeBytes(VARRTC.ADDRESS[DAYTWO], BYTLEN, VARRTC.ACTUALBYT);

        HMHIS(DAYTHREE);
      }
      VARRTC.STRCONV = "";
      eeprom.writeByte(VARRTC.ADDRESS[FLAG], VARRTC.DAYFLAG);
      Serial.println("DAY SAVED");
    }
  }
}

void LTCSEN()
{
  HMS();
  DAYSAVE();

  VARADC.ADV    = analogRead(ANIN);
  VARADC.EMA_S  = (VARADC.EMA_a * VARADC.ADV) + ((1 - VARADC.EMA_a) * VARADC.EMA_S);
  VARADC.VOUT   = (VARADC.EMA_S * 5.0) / 1024.0;
  VARADC.VIN    = VARADC.VOUT / (VARADC.RREF2 / (VARADC.RREF1 + VARADC.RREF2));

  if (VARADC.VIN < 1)
  {
    VARADC.VIN = 0;
  }

  lcd.setCursor(0, 0);
  lcd.print(String("Hometis ") + VARRTC.CURTIME);
  lcd.setCursor(0, 1);
  lcd.print(String((VARRTC.SVDHM + VARRTC.DELTATIME) / 3600.0) + '(' + VARRTC.ENGSTAT + ')');

  if (now.second() == 0) {
    lcd.clear();
  }

  RFLINK();
}

void BUFFRF(uint8_t VAL)
{
  strncpy(VARBUFF.RFDATE, VARCONV.SVDDATE[VAL], sizeof(VARBUFF.RFDATE));
  strncpy(VARBUFF.RFTIME, VARCONV.SVDTIME[VAL], sizeof(VARBUFF.RFTIME));
  VARBUFF.LSTHM = atof(VARCONV.SVDHMPDAY[VAL]);
  VARBUFF.ACTHM = atof(VARCONV.SVDHMACT[VAL]);
}

void RFLINK()
{
  if ((unsigned long)(millis() - PREVSEN) > SEND_RATE) {
    VARRTC.CURDATE.toCharArray(VARRF.RFDATE, sizeof(VARRF.RFDATE));
    VARRTC.CURTIME.toCharArray(VARRF.RFTIME, sizeof(VARRF.RFTIME));
    radio.write(&VARRF, sizeof(VARRF));

    Serial.println(String(VARRF.NOUNIT) + '|' + VARRF.stat + '|' + VARRF.RFDATE + '|' +
                   VARRF.RFTIME + '|' + VARRF.LSTHM + '|' + VARRF.ACTHM + '|');

    if (rfInterval <= 30 && (VARRTC.DAYFLAG & (1 << DAYONE | 1 << DAYTWO | 1 << DAYTHREE))) {
      if (rfInterval == 10 && (VARRTC.DAYFLAG & (1 << DAYTWO))) {
        //Serial.println("D2");
        VARBUFF.stat  = 4;
        BUFFRF(DAYTWO);
      }
      else if (rfInterval == 20 && (VARRTC.DAYFLAG & (1 << DAYTHREE))) {
        //Serial.println("D3");
        VARBUFF.stat  = 5;
        BUFFRF(DAYTHREE);
      }
      else if (rfInterval == 30) {
        //Serial.println("D1");
        VARBUFF.stat  = 3;
        BUFFRF(DAYONE);
        rfInterval = 0;
      }
      radio.write(&VARBUFF, sizeof(VARBUFF));
      Serial.println(String(VARBUFF.NOUNIT) + '|' + VARBUFF.stat + '|' + VARBUFF.RFDATE + '|' +
                     VARBUFF.RFTIME + '|' + VARBUFF.LSTHM + '|' + VARBUFF.ACTHM + '|');
      rfInterval++;
    }
    PREVSEN = millis();
  }
}

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

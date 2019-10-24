#include <Wire.h>
#include "RTClib.h"

#define HOURDIV 3600
#define MINDIV 60
#define X24C32 0x50
#define PRTIME 1000
#define DS3231_I2C_ADDRESS 0x68

DateTime now;
RTC_DS3231 rtc;

//Engine status & Sensor status
String ENGSTAT, TRIGSTAT;
unsigned long PREVSEN = 0;
unsigned long PREVSET = 0;

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
  uint8_t VAL = 0;
  uint8_t SEQ = 0;
  uint8_t I;
  char INBYTE;
  char MSG[20];
  byte IDX;
} SERCOM; SERCOM VARSER;

typedef struct
{
  String CURTIME;
  String CURDATE;
  uint8_t SEC;
  uint8_t MIN;
  uint8_t SEQ;
  uint32_t HR;
  char RCVDATA[15];
  unsigned long SVDHM;
  unsigned long LASTUNIX;
  unsigned long DELTATIME;
  unsigned long ELAPSED;
  bool LTCHM = false;

} RTCOM; RTCOM VARRTC;

void setup() {
  // put your setup code here, to run once:
  DDRD &= ~(1 << PIND3); //ACC
  DDRD &= ~(1 << PIND4); //DUMP
  DDRD &= ~(1 << PIND7); //ALT
  DDRD &= ~(1 << PIND5); //LOAD
  DDRD  |= (1 << PIND6);
  PORTD &= ~(1 << PIND3);
  PORTD &= ~(1 << PIND4);
  PORTD &= ~(1 << PIND7);
  PORTD &= ~(1 << PIND5);
  Wire.begin();
  rtc.begin();
  Serial.begin(9600);
  RDHMRTC();
}

void loop() {
  // put your main code here, to run repeatedly:
  PROG();
}

void SETRTC() {
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

  while (Serial.available() > 0) {
    char INCHAR = (char)Serial.read();
    VARSET.RTCIN += INCHAR;
    if (INCHAR == '\n') {
      VARSET.RTCPARSE = true;
    }
  }
  if (VARSET.RTCPARSE) {
    //Serial.print("data masuk: ");
    //Serial.print(VARSET.RTCIN);
    //Serial.print("\n");

    for (VARSET.CNTR = 1; VARSET.CNTR < VARSET.RTCIN.length(); VARSET.CNTR++) {
      if (VARSET.RTCIN[VARSET.CNTR] == '<') {
        VARSET.RTCDATA[VARSET.RTCIDX] = "";
      }
      else if ((VARSET.RTCIN[VARSET.CNTR] == '>') || (VARSET.RTCIN[VARSET.CNTR] == '|')) {
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

    //Serial.println(String("Detik: ") + second + "Menit: " + minute + "jam: " +
    //hour + "Hari: " + dayOfWeek + "tanggal: " + dayOfMonth + "Bulan: " + month + "Tahun: " + year + "VREF: " + VARALT.VREF);

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

void PARSETHM()
{
  while (Serial.available() > 0) {
    char INCHAR = (char)Serial.read();
    VARSER.DATAIN += INCHAR;
    if (INCHAR == '\n') {
      VARSER.INHM  = true;
    }
  }
  if (VARSER.INHM) {
    for (VARSER.I = 1; VARSER.I < VARSER.DATAIN.length(); VARSER.I++) {
      if (VARSER.DATAIN[VARSER.I] == '<') {
        VARSER.SETDATA[VARSER.SEQ] = "";
      }
      else if (VARSER.DATAIN[VARSER.I] == '>') {
        VARSER.SEQ++;
        VARSER.SETDATA[VARSER.SEQ] = "";
      }
      else {
        VARSER.SETDATA[VARSER.SEQ] = VARSER.SETDATA[VARSER.SEQ] + VARSER.DATAIN[VARSER.I];
      }
    }
    //B4 flushing(SETDATA[0]) need to be saved to rtc saving var
    VARRTC.SVDHM          = 0;
    VARRTC.SVDHM          = VARSER.SETDATA[0].toInt();
    //Serial.println(String("SVDHM: ") + VARRTC.SVDHM); //for debugging only
    HMWRT(VARRTC.SVDHM);
    VARSER.SEQ            = 0;
    VARSER.SETDATA[0]     = "";
    VARSER.DATAIN         = "";
  }
}

void LTCSEN()
{
  HMS();
  //for debugging only
  /*uint8_t PIN3ACC, PIN4DUMP, PIN7ALT, PIN5LOAD;
    PIN3ACC   = digitalRead(3);
    PIN4DUMP  = digitalRead(4);
    PIN7ALT   = digitalRead(7);
    PIN5LOAD  = digitalRead(5);*/


  if ((unsigned long)(millis() - PREVSEN) > PRTIME) {
    //for debugging only
    /*Serial.println(String("DELTATIME: ") + VARRTC.DELTATIME);
      Serial.println(String("ELAPSED: ") + VARRTC.ELAPSED);
      Serial.println(String("HMNOWSEC: ") + (VARRTC.SVDHM + VARRTC.DELTATIME));
      Serial.println(String("PIN3ACC: ") + PIN3ACC);
      Serial.println(String("PIN4DUMP: ") + PIN4DUMP);
      Serial.println(String("PIN7ALT: ") + PIN7ALT);
      Serial.println(String("PIN5LOAD: ") + PIN5LOAD);*/

    if ((PIND & (1 << PIND3)) && (!(PIND & (1 << PIND7))) && (!(PIND & (1 << PIND4))) && (!(PIND & (1 << PIND5))))
    {
      ENGSTAT   = "IN";
      TRIGSTAT  = "";
      Serial.print(String(VARRTC.CURDATE) + "|" + VARRTC.CURTIME + "|" + VARRTC.HR + ":" +
                   VARRTC.MIN + ":" + VARRTC.SEC + "|" + ENGSTAT + "|" + TRIGSTAT + "|");
    }
    else if ((PIND & (1 << PIND3)) && (PIND & (1 << PIND7)) && (!(PIND & (1 << PIND4))) && (!(PIND & (1 << PIND5))))
    {
      PORTD ^= (1 << PIND6);
      ENGSTAT = "RG";
      TRIGSTAT  = "";
      Serial.print(String(VARRTC.CURDATE) + "|" + VARRTC.CURTIME + "|" + VARRTC.HR + ":" +
                   VARRTC.MIN + ":" + VARRTC.SEC + "|" + ENGSTAT + "|" + TRIGSTAT + "|");
    }
    else if ((PIND & (1 << PIND3)) && (PIND & (1 << PIND7)) && (PIND & (1 << PIND4)) && (!(PIND & (1 << PIND5))))
    {
      ENGSTAT = "RG";
      TRIGSTAT = "DG";
      Serial.print(String(VARRTC.CURDATE) + "|" + VARRTC.CURTIME + "|" + VARRTC.HR + ":" +
                   VARRTC.MIN + ":" + VARRTC.SEC + "|" + ENGSTAT + "|" + TRIGSTAT + "|");
    }
    else if ((PIND & (1 << PIND3)) && (PIND & (1 << PIND7)) && (!(PIND & (1 << PIND4))) && (PIND & (1 << PIND5)))
    {
      ENGSTAT = "RG";
      TRIGSTAT = "LG";
      Serial.print(String(VARRTC.CURDATE) + "|" + VARRTC.CURTIME + "|" + VARRTC.HR + ":" +
                   VARRTC.MIN + ":" + VARRTC.SEC + "|" + ENGSTAT + "|" + TRIGSTAT + "|");
    }
    else {
      ENGSTAT = "IF";
      TRIGSTAT = "";
      Serial.print(String(VARRTC.CURDATE) + "|" + VARRTC.CURTIME + "|" + VARRTC.HR + ":" +
                   VARRTC.MIN + ":" + VARRTC.SEC + "|" + ENGSTAT + "|" + TRIGSTAT + "|");
    }
    PREVSEN = millis();
    Serial.println();
  }
}

void WRTBYTE(int DVCADDR, unsigned int EEADDR, byte DATA)
{
  int RDATA = DATA;
  Wire.beginTransmission(DVCADDR);
  Wire.write((int)(EEADDR >> 8));
  Wire.write((int)(EEADDR & 0xFF));
  Wire.write(RDATA);
  Wire.endTransmission();
}

void WRTPAGE(int DVCADDR, unsigned int ADDRPG, byte* DATA, byte LENGTH)
{
  Wire.beginTransmission(DVCADDR);
  Wire.write((int)(ADDRPG >> 8));
  Wire.write((int)(ADDRPG & 0xFF));
  byte DATASEQ;
  for (DATASEQ = 0; DATASEQ < LENGTH; DATASEQ++)
    Wire.write(DATA[DATASEQ]);
  Wire.endTransmission();
}

byte RDBYTE(int DVCADDR, unsigned int EEADDR)
{
  byte RDATA  = 0xFF;
  Wire.beginTransmission(DVCADDR);
  Wire.write((int)(EEADDR >> 8));
  Wire.write((int)(EEADDR & 0xFF));
  Wire.endTransmission();
  Wire.requestFrom(DVCADDR, 1);
  if (Wire.available()) RDATA = Wire.read();
  return RDATA;
}

void RDBUF(int DVCADDR, unsigned int EEADDR, byte *BUFFER, int LENGTH)
{
  Wire.beginTransmission(DVCADDR);
  Wire.write((int)(EEADDR >> 8));
  Wire.write((int)(EEADDR & 0xFF));
  Wire.endTransmission();
  Wire.requestFrom(DVCADDR, LENGTH);
  int DATASEQ = 0;
  for (DATASEQ = 0; DATASEQ < LENGTH; DATASEQ++)
    if (Wire.available()) BUFFER[DATASEQ] = Wire.read();
}

void HMS()
{
  now = rtc.now();

  if ((PIND & (1 << PIND3)) && (PIND & (1 << PIND7))) {
    if (!VARRTC.LTCHM) {
      RDHMRTC();
      VARRTC.ELAPSED += VARRTC.DELTATIME;
      VARRTC.SVDHM += VARRTC.ELAPSED;
      //Serial.println(String("SVDHM+DELTATIME: ") + VARRTC.SVDHM); //for debugging only
      VARRTC.LASTUNIX   = 0;
      VARRTC.LASTUNIX   = now.unixtime();
      VARRTC.LTCHM      = true;
    }
    VARRTC.DELTATIME = now.unixtime() - VARRTC.LASTUNIX;
  }
  else {
    VARRTC.LTCHM     = false;
  }
  //saat nyala lg waktunya blm update (SVDHM) //closed need to test NOTE: RESET DELTATIME in 2
  VARRTC.CURTIME  = String(now.hour()) + ":" + now.minute() + ":" + now.second();
  VARRTC.CURDATE  = String(now.year()) + "-" + now.month() + "-" + now.day();
  VARRTC.SEC      = (VARRTC.SVDHM + VARRTC.DELTATIME) % MINDIV;
  VARRTC.MIN      = ((VARRTC.SVDHM + VARRTC.DELTATIME) % HOURDIV) / MINDIV;
  VARRTC.HR       = (VARRTC.SVDHM + VARRTC.DELTATIME) / HOURDIV;
}

void RDHMRTC()
{
  uint8_t ADDR  = 0;
  byte MEMADDR = RDBYTE(X24C32, 0);

  while (MEMADDR != 0)
  {
    VARRTC.RCVDATA[VARRTC.SEQ] = MEMADDR;
    ADDR++;
    VARRTC.SEQ++;
    MEMADDR = RDBYTE(X24C32, ADDR);
    Serial.println("tes");
  }
  VARRTC.SEQ = 0;
  String STRHM(VARRTC.RCVDATA);
  VARRTC.SVDHM = STRHM.toInt();
  //Serial.println(String("Read HM RTC: ") + VARRTC.SVDHM); //for debugging only
}

void HMWRT(unsigned long DATA2CONV)
{
  char DATA2WRT[12];
  String STRHM;
  STRHM = String(DATA2CONV);
  STRHM.toCharArray(DATA2WRT, sizeof(DATA2WRT));
  //Serial.println(String("DATA2WRT: ") + DATA2WRT); //for debugging only
  WRTPAGE(X24C32, 0, (byte *)DATA2WRT, sizeof(DATA2WRT));
  delay(100);
  //Serial.println("DONE SAVE"); //for debugging only
}

void PROG()
{
  LTCSEN();
  LTCSER();

  switch (VARSER.VAL) {
    case 3:
      SERFLUSH();
      VARRTC.DELTATIME  = 0;
      VARRTC.ELAPSED    = 0;
      while (!VARSER.INHM) {
        if ((unsigned long)(millis() - PREVSET) > PRTIME) {
          Serial.println('#');
          PORTD ^= (1 << PIND6);
          PARSETHM();
          PREVSET = millis();
        }
      }
      //Serial.println("DONE SET HM"); //for debugging only
      PORTD &= ~(1 << PIND6);
      VARSER.INHM = false;
      VARSER.VAL = 0;

      break;

    case 2:
      if ((PIND & (1 << PIND3)) && (PIND & (1 << PIND7))) {
        HMWRT(VARRTC.SVDHM + VARRTC.DELTATIME);
      } else {
        HMWRT(VARRTC.SVDHM + VARRTC.ELAPSED + VARRTC.DELTATIME);
      }
      VARRTC.ELAPSED    = 0;
      VARSER.VAL        = 0;

      break;

    case 1:
      SERFLUSH();
      while (!VARSET.RTCPARSE)
        if ((unsigned long)(millis() - PREVSET) > PRTIME) {
          Serial.println('&');
          PORTD ^= (1 << PIND6);
          SETRTC();
          PREVSET = millis();
        }
      PORTD &= ~(1 << PIND6);
      VARSET.RTCPARSE = false;
      VARSER.VAL = 0;

      break;
  }
}

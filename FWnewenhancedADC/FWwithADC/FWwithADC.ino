/*
   FMS (Fleet Management System)
   Build Date   : 01/10/2019
   Last Update  : 30/12/2019
*/

#include <Wire.h>
#include "RTClib.h"

// BOOTIME VARSH.WAITRPY == BOOTIME + 1
#define BOOTIME 180                // BOOTING TIME LATTE
#define BUZINTRVL 600              // BUZZER INTERVAL ALARM
#define PREBUZON  2                // PRE BUZZER ON TIME 
#define HOURDIV 3600               // HOUR DIV
#define MINDIV 60                  // MINUTE DIV
#define X24C32 0x57                // RTC EEPROM ADDR
#define PRTIME 1000                // INTERVAL DELAY
#define DS3231_I2C_ADDRESS 0x68    // RTC ADDR
#define ADCACC 0                   // ACC ADDR INDEX COMPARATOR
#define ADCALT 1                   // ALT ADDR INDEX COMPARATOR
#define ADCDUMP 2                  // DUMP ADDR INDEX COMPARATOR

DateTime now;
RTC_DS3231 rtc;

//ENGINE STATUS, SENSOR STATUS & FW VERSION
String FWVERS = "HWD_RND|FMSIO_v1.0|AGM";
String ENGSTAT, TRIGSTAT;
unsigned long PREVSEN = 0;
unsigned long PREVSET = 0;
unsigned long PREVSH  = 0;
const uint8_t NA      = 3;

typedef struct
{
  const uint8_t AIN[NA] = {A1, A2, A3};
  const uint8_t VREF    = 10;
  uint16_t ADV[3]       = {0, 0, 0};
  uint16_t EMA_S[3]     = {0, 0, 0};
  float VIN[3]          = {0.0, 0.0, 0.0};
  float VOUT[3]         = {0.0, 0.0, 0.0};
  float EMA_a           = 0.6;
  float RREF1           = 1000000.0;
  float RREF2           = 100000.0;
} ADCPROG; ADCPROG VARADC;

typedef struct
{
  uint8_t BTFAIL  = 0;
  uint8_t WAITSH  = 0;
  uint8_t WAITACC = 0;
  uint8_t WAITRPY = 0;
  uint8_t SHCODE  = 0;
  uint8_t WARNED  = 0;
  uint16_t HANG   = 0;
  bool LOGSTATE   = false;
  bool HANGSTATE  = false;
} LATTESH; LATTESH VARSH;

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

void SETRTC(void);
byte decToBcd(byte val);
void PROG(void);
void LTCWARN(void);
void SHPC(void);
void HMWRT(unsigned long DATA2CONV);
void RDHMRTC(void);
void HMS(void);
void RDBUF(int DVCADDR, unsigned int EEADDR, byte *BUFFER, int LENGTH);
byte RDBYTE(int DVCADDR, unsigned int EEADDR);
void WRTBYTE(int DVCADDR, unsigned int EEADDR, byte DATA);
void LTCSEN(void);
void PARSETHM(void);
void LTCSER(void);
void SERFLUSH(void);

void setup() {
  // put your setup code here, to run once:
  DDRB |= (1 << PINB0); //RELAY
  PORTB |= (1 << PINB0);
  DDRB |= (1 << PINB1); //BUZZER
  DDRB &= ~(1 << PINB2); //BUTTON
  PORTB |= (1 << PINB2);
  DDRD &= ~(1 << PIND5); //LOAD
  PORTD &= ~(1 << PIND5);
  DDRD |= (1 << PIND4); //RST ENABLE/DISABLE
  PORTD |= (1 << PIND4); //RST DISABLED
  DDRD  |= (1 << PIND6); //LED INDICATOR
  Wire.begin();
  rtc.begin();
  Serial.begin(9600);
  RDHMRTC(); //RETURNING HM VALUE
}

void loop() {
  // put your main code here, to run repeatedly:
  PROG();
}


/*
  =======================================================================
  =========================   SETTING RTC TIME  =========================
  =======================================================================
*/
void SETRTC() {
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
    byte second      = VARSET.RTCDATA[0].toInt();
    byte minute      = VARSET.RTCDATA[1].toInt();
    byte hour        = VARSET.RTCDATA[2].toInt();
    byte dayOfWeek   = VARSET.RTCDATA[3].toInt();
    byte dayOfMonth  = VARSET.RTCDATA[4].toInt();
    byte month       = VARSET.RTCDATA[5].toInt();
    byte year        = VARSET.RTCDATA[6].toInt();

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
    VARRTC.SVDHM          = 0;
    VARRTC.SVDHM          = VARSER.SETDATA[0].toInt();
    //Serial.println(String("SVDHM: ") + VARRTC.SVDHM); //for debugging only
    HMWRT(VARRTC.SVDHM);
    VARSER.SEQ            = 0;
    VARSER.SETDATA[0]     = "";
    VARSER.DATAIN         = "";
  }
}


/*
  ============================================================================
  =========================   SENSOR STATE program   =========================
  ============================================================================
*/
void LTCSEN()
{
  HMS();
  //for debugging only
  for (int i = 0; i < NA; i++)
  {
    VARADC.ADV[i]    = analogRead(VARADC.AIN[i]);
    VARADC.EMA_S[i]  = (VARADC.EMA_a * VARADC.ADV[i]) + ((1 - VARADC.EMA_a) * VARADC.EMA_S[i]);
    VARADC.VOUT[i]   = (VARADC.EMA_S[i] * 5.0) / 1024.0;
    VARADC.VIN[i]    = VARADC.VOUT[i] / (VARADC.RREF2 / (VARADC.RREF1 + VARADC.RREF2));

    if (VARADC.VIN[i] < 1)
    {
      VARADC.VIN[i] = 0;
    }
  }

  if ((unsigned long)(millis() - PREVSEN) > PRTIME) {
    //for debugging only
    //Serial.println(String("DELTATIME: ") + VARRTC.DELTATIME);
    //Serial.println(String("ELAPSED: ") + VARRTC.ELAPSED);
    //Serial.println(String("HMNOWSEC: ") + (VARRTC.SVDHM + VARRTC.DELTATIME));
    //Serial.println(String("WAIT ACC: ") + VARSH.WAITACC);
    //Serial.println(String("WAIT SH: ") + VARSH.WAITSH);
    //Serial.println(String("WAIT REPLY: ") + VARSH.WAITRPY);
    //Serial.println(String("SH CODE: ") + VARSH.SHCODE);*/

    if (VARADC.VIN[ADCACC] > VARADC.VREF || VARADC.VIN[ADCALT] > VARADC.VREF)
    {
      if (VARADC.VIN[ADCACC] > VARADC.VREF && VARADC.VIN[ADCALT] < VARADC.VREF)
      {
        ENGSTAT = "IN";
        TRIGSTAT = "";
      }
      else
      {
        ENGSTAT = "RG";
        TRIGSTAT = "";
        PORTD ^= (1 << PIND6);
        if (VARADC.VIN[ADCDUMP] > VARADC.VREF)
        {
          TRIGSTAT = "DG";
        }
        else if (PIND & (1 << PIND5))
        {
          TRIGSTAT = "LG";
        }
      }
    }

    else {
      ENGSTAT = "IF";
      TRIGSTAT = "";
      VARSH.WAITACC = 0;
    }
    Serial.println(String(VARRTC.CURDATE) + "|" + VARRTC.CURTIME + "|" + VARRTC.HR + ":" +
                   VARRTC.MIN + ":" + VARRTC.SEC + "|" + ENGSTAT + "|" + TRIGSTAT + "|");
    PREVSEN = millis();
  }
}


/*
  =======================================================================
  =========================   RTC R/W program   =========================
  =======================================================================
*/
/*void WRTBYTE(int DVCADDR, unsigned int EEADDR, byte DATA)
{
  int RDATA = DATA;
  Wire.beginTransmission(DVCADDR);
  Wire.write((int)(EEADDR >> 8));
  Wire.write((int)(EEADDR & 0xFF));
  Wire.write(RDATA);
  Wire.endTransmission();
}*/

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
  Wire.endTransmission();
  Wire.requestFrom(DVCADDR, 1);
  if (Wire.available()) RDATA = Wire.read();
  return RDATA;
}

/*void RDBUF(int DVCADDR, unsigned int EEADDR, byte *BUFFER, int LENGTH)
{
  Wire.beginTransmission(DVCADDR);
  Wire.write((int)(EEADDR >> 8));
  Wire.write((int)(EEADDR & 0xFF));
  Wire.endTransmission();
  Wire.requestFrom(DVCADDR, LENGTH);
  int DATASEQ = 0;
  for (DATASEQ = 0; DATASEQ < LENGTH; DATASEQ++)
    if (Wire.available()) BUFFER[DATASEQ] = Wire.read();
}*/


/*
  =======================================================================
  =========================      HM program     =========================
  =======================================================================
*/
void HMS()
{
  now = rtc.now();

  if (VARADC.VIN[ADCALT] > VARADC.VREF) {
    if (!VARRTC.LTCHM) {
      //Serial.println(String("SAVED HM ALTON : ") + VARRTC.SVDHM);
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
    if (VARRTC.LTCHM) {
      //Serial.println("SAVING...");
      HMWRT(VARRTC.SVDHM + VARRTC.DELTATIME);
      VARRTC.ELAPSED    = 0;
      VARRTC.DELTATIME  = 0;
      VARRTC.SVDHM      = 0;
      RDHMRTC();
      VARRTC.LTCHM     = false;
      //Serial.println("SAVED...");
      //Serial.println(String("SAVED HM ALTON : ") + VARRTC.SVDHM);
    }
  }
  //saat nyala lg waktunya blm update (SVDHM) //closed need to test NOTE: RESET DELTATIME in 2
  VARRTC.CURTIME  = String(now.hour()) + ":" + now.minute() + ":" + now.second();
  VARRTC.CURDATE  = String(now.year()) + "-" + now.month() + "-" + now.day();
  VARRTC.SEC      = (VARRTC.SVDHM + VARRTC.DELTATIME) % MINDIV;
  VARRTC.MIN      = ((VARRTC.SVDHM + VARRTC.DELTATIME) % HOURDIV) / MINDIV;
  VARRTC.HR       = (VARRTC.SVDHM + VARRTC.DELTATIME) / HOURDIV;
}


/*
  ===========================================================================
  =========================   RTC R/W sub program   =========================
  ===========================================================================
*/
//READ HM FROM RTC
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
    //Serial.println("tes");
  }
  VARRTC.SEQ = 0;
  String STRHM(VARRTC.RCVDATA);
  VARRTC.SVDHM = STRHM.toInt();
  //Serial.println(String("Read HM RTC: ") + VARRTC.SVDHM); //for debugging only
}

//WRITE HM TO RTC
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


/*
  ==========================================================================
  =========================  SHUT DOWN PC program  =========================
  ==========================================================================
*/
void SHPC()
{
  if ((VARADC.VIN[ADCACC] > VARADC.VREF) || (VARADC.VIN[ADCALT] > VARADC.VREF) || (VARSH.WAITACC == 6) || (VARSH.WAITRPY == 181)) {
    if (VARSH.WAITACC <= 5) {
      if ((unsigned long)(millis() - PREVSH) > PRTIME) {
        //Serial.print("TES 0: ");
        //Serial.println(VARSH.WAITACC);
        VARSH.WAITACC++;
        PREVSH = millis();
      }
    }
    else {
      if (VARSH.WAITSH <= 5) {
        VARSH.WAITRPY = 0;
        PORTB &= ~(1 << PINB0);
        if ((unsigned long)(millis() - PREVSH) > PRTIME) {
          VARSH.WAITSH++;
          PREVSH = millis();
        }
      }
      else {
        PORTB |= (1 << PINB0);
        //rcv condition code(2) from latte
        if (VARSH.WAITRPY <= BOOTIME) {
          if ((unsigned long)(millis() - PREVSH) > PRTIME) {
            VARSH.WAITRPY++;
            PREVSH = millis();
          }
        }
        else {
          PORTB |= (1 << PINB1);
          VARSH.LOGSTATE = true;
          VARSH.WAITACC = 0;
          VARSH.WAITSH  = 0;
          VARSH.BTFAIL++;
        }
      }
    }
  }
}


/*
  =========================================================================
  =========================  LOGIN STATE program  =========================
  =========================================================================
*/
void LTCWARN()
{
  if (!VARSH.LOGSTATE && !VARSH.HANGSTATE) {
    if (!(PINB & (1 << PINB2))) {
      PORTB &= ~(1 << PINB1);
      VARSH.HANGSTATE  = true;
    }

    if (VARADC.VIN[ADCALT] > VARADC.VREF) {
      if ((unsigned long)(millis() - PREVSET) > PRTIME) {
        if (VARSH.WARNED >= PREBUZON) {
          PORTB ^= (1 << PINB1);
        }
        VARSH.WARNED++;
        PREVSET = millis();
      }
    }
    else {
      PORTB &= ~(1 << PINB1);
      VARSH.WARNED = 0;
    }
  }

  else if (!VARSH. LOGSTATE && VARSH.HANGSTATE) {
    if ((unsigned long)(millis() - PREVSET) > PRTIME) {
      VARSH.HANG++;
      if (VARSH.HANG <= 3) {
        Serial.println('@');
      }
      if (VARSH.HANG == BUZINTRVL) {
        VARSH.HANGSTATE   = false;
        VARSH.HANG        = 0;
      }
      PREVSET = millis();
    }
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
  LTCWARN();

  switch (VARSER.VAL) {
    //LOGIN STATE
    case 6:
      Serial.println("RST ENABLED");
      PORTD &= ~(1 << PIND4);
      VARSER.VAL      = 0;

      break;

    case 5:
      PORTB &= ~(1 << PINB1);
      VARSH.LOGSTATE  = true;
      VARSH.HANGSTATE = false;
      VARSH.HANG      = 0;
      VARSH.WARNED    = 0;
      VARSER.VAL      = 0;
      break;

    //SHUT DOWN PC
    case 4:
      if (VARSH.SHCODE < 5) { 
        if ((unsigned long)(millis() - PREVSET) > PRTIME) {
          VARSH.SHCODE++;
          Serial.println('$');
          PREVSET = millis();
        }
        VARSH.BTFAIL  = 0;
      }
      else {
        SHPC();
        if (VARSH.BTFAIL != 0) {
          if ((unsigned long)(millis() - PREVSET) > PRTIME) {
            Serial.println(String("F$") + VARSH.BTFAIL);
            PREVSET = millis();
          }
        }
      }
      break;

    //SET HM
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

    //LOGOUT, SAVE HM, PC STATE DETECTION
    case 2:
      VARSH.WAITACC   = 0;
      VARSH.WAITSH    = 0;
      VARSH.WAITRPY   = 0;
      VARSH.SHCODE    = 0;
      VARSH.WARNED    = 0;
      VARSH.BTFAIL    = 0;
      VARSH.LOGSTATE  = false;
      VARSER.VAL      = 0;

      break;

    //SET RTC
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

//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
#include <ds3231.h>
#include <PowerSaver.h>
#include <SdFat.h>
#include <dht.h>
#include <Wire.h>
#include <EEPROM.h>

// RTC    ******************************
#define SECONDS_DAY 86400
#define BUFF_MAX 96
//char buff[BUFF_MAX];
//int dayStart = 26, hourStart = 20, minStart = 30;    // start time: day of the month, hour, minute (values automatically assigned by the GUI)
//const uint8_t days_in_month [12] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };
ISR(PCINT0_vect)  // Setup interrupts on D8; Interrupt (RTC SQW) 
{
  PORTB ^= (1<<PORTB1);
}

// SD card    ******************************
SdFat sd;
SdFile myFile;
char sdLogFile[] = "sdlog.csv";
char configFile[] = "index.txt";
#define SDcsPin 9 // D9

// DHT    ******************************
#define DHT22_PIN 6 // D6
dht DHT;

// NPN    ******************************
#define NPN1 3 // D3 control ESP8266
#define NPN2 4 // D4 control DHT & SD Card

// User Configuration
uint16_t captureInt = 15, uploadInt = 30;  // in seconds

// Low Power 
PowerSaver chip;  // declare object for PowerSaver class

// Control flag
uint32_t captureCount = 0, uploadCount = 0, uploadedLines = 0;
uint32_t nextCaptureTime, nextUploadTime, alarm;
struct ts t;

// LED
#define LED 5

// regulator
#define LDO 5

// wifi
//#define SSID "iPhone"  //change to your WIFI name
//#define PASS "s32nzqaofv9tv"  //wifi password
#define CONCMD1 "AT+CWMODE=1"
//#define CONCMD2 "AT+CWJAP=\"iPhone\",\"s32nzqaofv9tv\"" // iPhone is SSID, s32*** is password
#define IPcmd "AT+CIPSTART=\"TCP\",\"184.106.153.149\",80" // ThingSpeak IP Address: 184.106.153.149
//String GET = "GET /update?key=8LHRO7Q7L74WVJ07&field1=";
//char wifiName[10]; 
//char wifiPass[16];
//char API[48];
//#define wifiName "iPhone"
//#define wifiPass "s32nzqaofv9t"
//#define wifiName "HOUSTON_23"
//#define wifiPass "0450850509_UNSW"
//#define API "GET /update?key=8LHRO7Q7L74WVJ07&field1="


// setup ****************************************************************
void setup()
{
    initialize();
    //readUserSetting();
    readUserSettingEEPROM();
    //echoEEPROM();
    //testMemSetup();
    chip.sleepInterruptSetup();    // setup sleep function on the ATmega328p. Power-down mode is used here
    nextCaptureTime = t.unixtime;
    nextUploadTime = t.unixtime;
}

void echoEEPROM() {
  // for (int address = 0; address < 100; address++) {
  //     int value = EEPROM.read(address);
  //     Serial.print(address);
  //     Serial.print("\t");
  //     Serial.print(value, DEC);
  //     Serial.println();
  //     delay(500);
  // }
//Serial.println(getALlEEPROM());
  // Serial.println(getWiFiName());
  // Serial.println(getWifiPass());
    getWifiPass();
    getAPI();
  // Serial.println(getAPI());
}

void loop()
{
    DS3231_get(&t);
    // debugPrintTime();
    // testCaptureData();
    // testMemSetup();
    // testUploadData();
    // testWiFi();
    if (isCaptureMode()) {
        Serial.println(F("CaptureMode"));
        digitalWrite(NPN2, HIGH);
        captureStoreData();
        captureCount++;
        DS3231_get(&t);
        while (nextCaptureTime < t.unixtime) nextCaptureTime += captureInt;
    } else if (isUploadMode()) {
        Serial.println(F("UploadMode"));
        digitalWrite(NPN1, HIGH);
        uploadData();
        uploadCount++;
        DS3231_get(&t);
        while (nextUploadTime < t.unixtime) nextUploadTime += uploadInt;
    } else if (isSleepMode()) {
        Serial.println(F("SleepMode"));
        setAlarm1();
        goSleep();
    }
}

void goSleep()
{
    //Serial.println(F("goSleep"));
    delay(2000);
    digitalWrite(NPN2, LOW);
    digitalWrite(NPN1, LOW);
    delay(5);  // give some delay
    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
    chip.goodNight();
    if (DS3231_triggered_a1()) {
        delay(1000);
        //debugPrintTime();
        //Serial.println(F("**Alarm has been triggered**"));
        DS3231_clear_a1f();
    }
    //chip.turnOnADC();    // enable ADC after processor wakes up
    chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
    delay(1);    // important delay to ensure SPI bus is properly activated
    //RTC.alarmFlagClear();
}

void readUserSetting()
{
    const int line_buffer_size = 100;
    char buffer[line_buffer_size];
    int line_number = 0;
    int num = 0;
    ifstream sdin(configFile);

    if (!myFile.open(configFile, O_READ)) {
        sd.errorHalt("sd!");
    }

    while (sdin.getline(buffer, line_buffer_size, ',') || sdin.gcount()) {
        num = num + 1;
        if (num == 1){
          //wifiName = (String) buffer;
            //strncpy(wifiName, buffer, 16);
        }else if (num == 2){
          //wifiPassword = (String) buffer;
            //strncpy(wifiPass, buffer, 16);
        }else if (num == 3){
          //API = (String) buffer;
          //strncpy(API, buffer, 40);
        }else if (num == 4){
            captureInt = atof(buffer);
        }else if (num == 5){
            uploadInt = atof(buffer);
        }   
    }
    myFile.close();
}

void readUserSettingEEPROM()
{
    int i = 0;
    int addr = 0;
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(configFile, O_READ)) {
        sd.errorHalt("opening test.txt for read failed");
    }

    //Serial.println(F("readUserSettingEEPROM"));
    while (myFile.available()) {
        i = myFile.read();
        //Serial.write(i);
        EEPROM.write(addr++, i);
        delay(100);
    }
    myFile.close();

}

void initialize()
{
    Serial.begin(9600); // Todo: try 19200
    delay(1000);
    pinMode(LED, OUTPUT);
    pinMode(NPN2, OUTPUT); // Turn on DHT & SD
    pinMode(NPN1, OUTPUT); // Turn on WiFi  
    digitalWrite(NPN2, HIGH);
    digitalWrite(NPN1, HIGH);
    digitalWrite(LED, LOW);

    // 1. check SD
    // initialize SD card on the SPI bus
    //todo: get the line number to ignore
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_WRITE | O_CREAT | O_TRUNC)) {
        sd.errorHalt("sd!");
    }
    myFile.close();
    
    // 2. check wifi connection

    // RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a1f();
    DS3231_get(&t);
    nextCaptureTime = t.unixtime + captureInt;
    nextUploadTime = t.unixtime + uploadInt;

    // 3. check DHT
    // 4. check battery
    // 5. check USB connection
}

bool isCaptureMode()
{
  return int32_t(nextCaptureTime - t.unixtime) < 1;
}

bool isUploadMode()
{
    return int32_t(nextUploadTime - t.unixtime) < 1;
}

bool isSleepMode()
{
    uint32_t capAlarm, upAlarm;
    capAlarm = nextCaptureTime - t.unixtime;
    upAlarm = nextUploadTime - t.unixtime;
    alarm = (capAlarm<upAlarm) ? capAlarm : upAlarm;
    // sleep&wake needs at most 4 sec so any value below is not worth while
    return alarm>4;
}

void testCaptureData()
{
    //char raw[64] = "1,2015-02-27,01:44:33,38.5,66.6$";
    char ttmp[8]; 
    char htmp[8];
    float temp, hum;
    String str;
    delay(5000);
//    dtostrf(27.5, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    dtostrf(-3.0, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    Serial.println(F("testCaptureData"));
    int chk = DHT.read22(DHT22_PIN);
    //Serial.print("chk: "); Serial.print(chk);
    temp = DHT.temperature;
    hum = DHT.humidity;
    captureCount++;
    dtostrf(temp, 3, 1, ttmp);
    dtostrf(hum, 3, 1, htmp);
    str += String(captureCount) + "," + String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
        String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + "," + String(ttmp) + "," + String(htmp) + String("$");
        
    delay(1000);
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        sd.errorHalt("sd!");
    }
    //myFile.println("testing 1, 2, 3.");
    myFile.println(str);
    // close the file:
    myFile.close();
}

void captureStoreData()
{
    //char raw[64] = "9999999,2015-02-27,01:44:33,-38.5,66.6$";
    char ttmp[8]; 
    char htmp[8];
    String str;
    float temp, hum;
    int chk;
    chk = DHT.read22(DHT22_PIN);
    temp = DHT.temperature;
    hum = DHT.humidity;
    dtostrf(temp, 3, 1, ttmp);
    dtostrf(hum, 3, 1, htmp);
    // str += String(captureCount) + "," + String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
    //     String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + "," + String(ttmp) + "," + String(htmp) + String("$");
        
    delay(1000);
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        //sd.errorHalt("sd!");
    }
    //myFile.println(str);
    myFile.print(captureCount);myFile.print(F(","));
    myFile.print(t.year); myFile.print(F("-"));myFile.print(t.mon);myFile.print(F("-"));myFile.print(t.mday);myFile.print(F(","));
    myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec); myFile.print(F(","));
    myFile.print(ttmp); myFile.print(F(",")); myFile.print(htmp); myFile.println(F("$"));

    myFile.close();
    captureBlink();
    
//    if (!myFile.open(sdLogFile, O_READ)) {
//        sd.errorHalt("opening failed");
//    }
//    Serial.println(sdLogFile);
//
//    // read from the file until there's nothing else in it:
//    int data;
//    while ((data = myFile.read()) >= 0) Serial.write(data);
//    // close the file:
//    myFile.close(); 
}

void uploadData()
{
    String stemp, shum, stime, data;
    uint8_t i = 0;
    int lineNum = 0;
    const int line_buffer_size = 64;
    char buffer[line_buffer_size];
    while (!Serial.find("OK")) {
        if (i++>50) return;
        Serial.println(F("AT"));
        delay(2000);
    }

    //Serial.println(F("connectWiFi"));
    if (connectWiFi()) {
        // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with breadboards. use SPI_FULL_SPEED for better performance.
        if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
        ifstream sdin(sdLogFile);         

        while (sdin.getline(buffer, line_buffer_size, '\n') || sdin.gcount()) {
          //int count = sdin.gcount();
          if (sdin.fail()) {
            //cout << "Partial long line";
            //Serial.println(F("Partial long line"));
            sdin.clear(sdin.rdstate() & ~ios_base::failbit);
          } else if (sdin.eof()) {
            //cout << "Partial final line";  // sdin.fail() is false
            //Serial.println(F("Partial final line"));
          } else {
            lineNum++;
          }
          if (lineNum<=uploadedLines) continue;

          stemp = getTemp(buffer);
          shum = getHum(buffer);
          stime = getTime(buffer);
          //https://api.thingspeak.com/update?api_key=8LHRO7Q7L74WVJ07&field1=33&field2=3&created_at=2015-02-27%2012:43:00
          data = stemp + String(F("&field2=")) + shum + String(F("&created_at=")) + stime; //+ String(F("+11:00"));                        
          transmitData(data);
          delay(9000);
        }
    }
}

void setAlarm1()
{
    uint32_t dayclock, wakeupTime;
    uint8_t second, minute, hour;
    wakeupTime = t.unixtime + alarm;
    dayclock = (uint32_t)wakeupTime % SECONDS_DAY;

    second = dayclock % 60;
    minute = (dayclock % 3600) / 60;
    hour = dayclock / 3600;

    // flags define what calendar component to be checked against the current time in order
    // to trigger the alarm - see datasheet
    // A1M1 (seconds) (0 to enable, 1 to disable)
    // A1M2 (minutes) (0 to enable, 1 to disable)
    // A1M3 (hour)    (0 to enable, 1 to disable) 
    // A1M4 (day)     (0 to enable, 1 to disable)
    // DY/DT          (dayofweek == 1/dayofmonth == 0)
    boolean flags[5] = { 0, 0, 0, 1, 1};

    // set Alarm1
    DS3231_set_a1(second, minute, hour, 0, flags);
    // Serial.print(F("Hour: "));Serial.println(hour);
    // Serial.print(F("Min: "));Serial.println(minute);
    // Serial.print(F("Second: "));Serial.println(second);

    // activate Alarm1
    DS3231_set_creg(DS3231_INTCN | DS3231_A1IE);
}

boolean connectWiFi(){
    uint8_t i = 0;
    String wifiName, wifiPass;
    wifiName = getWiFiName();
    wifiPass = getWifiPass();
    Serial.println(F(CONCMD1));
    delay(2000);
    Serial.print(F("AT+CWJAP=\"")); Serial.print(wifiName); Serial.print(F("\",\"")); Serial.print(wifiPass); Serial.println(F("\""));
    //Serial.println(String(F("AT+CWJAP=\"")) + String(wifiName) + String(F("\",\"")) + String(wifiPass) + String(F("\"")));
    delay(3000);
    //testMemSetup();
    while (Serial.find("OK")) {
        if (i++>50) return false;
        delay(3000);
    }
    return true;
}

void debugPrintTime()
{
    // display current time
    String str;
    DS3231_get(&t);
    //snprintf(buff, BUFF_MAX, "%02d:%02d:%02d,cap:%d,up:%d,",t.hour, t.min, t.sec, captureCount, uploadCount);
    //Serial.println(buff);
    str += String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
        String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + ",cap:" + String(captureCount) + ",up:" + String(uploadCount);
    Serial.println(str);
}

void transmitData(String data) {  
    //String cmd(GET);
    //String cmd(F(API));
    String cmd;
    int length;

    uint8_t i = 0;
    Serial.println(F(IPcmd));
    delay(2000);
    if(Serial.find("Error")) return;

    cmd = getAPI();
    // cmd += data;
    // cmd += "\r\n";
    length = cmd.length() + data.length() + 2;
    Serial.print(F("AT+CIPSEND="));
    Serial.println(length);
    delay(5000);

    while (!Serial.find(">")) {
        if (i++>100) return;
        Serial.println(F("AT+CIPCLOSE"));
        delay(2000);
        Serial.println(F("AT"));
        delay(2000);
        while (!Serial.find("OK")) {
            if (i++>100) return;
            delay(2000);
            Serial.println(F("AT"));
        }
        if (connectWiFi()) {
            Serial.println(F(IPcmd));
            if(Serial.find("Error")) return;
            delay(5000);
            Serial.print(F("AT+CIPSEND="));
            //Serial.println(cmd.length());
            Serial.println(length);
            delay(5000);
        }
    }
    Serial.print(cmd); Serial.print(data); Serial.print(F("\r\n"));
    //Serial.print(cmd);
    uploadedLines++;
    uploadBlink();

    //testMemSetup();
    return;
  // if(Serial.find(">")){
  //   Serial.print(cmd);
  //   uploadedLines++;
  // }else{
  //   Serial.println(F("AT+CIPCLOSE"));
  // }
}

String getALlEEPROM()
{ 
    char val;
    int i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != '$') {
        str += val;
        val = EEPROM.read(i++);
    }
    return str;
}

String getWiFiName()
{
    char val;
    int i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != ',') {
        str += val;
        val = EEPROM.read(i++);
    }
    return str;
}

String getWifiPass()
{
    char val;
    int count = 0, sp = 0, ep = 0, i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != '$') {
        if (val == ',') count++;
        if (count == 1 && sp == 0) {sp = i;}
        if (count == 2 && ep == 0) {ep = i-2;}
        val = EEPROM.read(i++);
    }
    // Serial.println(sp);
    // Serial.println(ep);
    // return "";
    for (int i = sp; i != ep + 1; i++) {
        val = EEPROM.read(i);
        delay(35);
        str += val;
    }
    //Serial.println(str);
        
    return str;
}

String getAPI()
{
    char val;
    int count = 0, sp = 0, ep = 0, i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != '$') {
        if (val == ',') count++;
        if (count == 2 && sp == 0) {sp = i;}
        if (count == 3 && ep == 0) {ep = i-2;}
        val = EEPROM.read(i++);
    }
    //     Serial.println(sp);
    // Serial.println(ep);

    // return "";
    for (int i = sp; i != ep + 1; i++) {
        val = EEPROM.read(i);
        delay(35);
        str += val;
    }
    //Serial.println(str);
    return str;
}

String getTemp(char *buf)
{
    // Will copy 18 characters from array1 to array2
    //strncpy(array2, array1, 18);
    int count = 0, sp = 0, ep = 0;
    String str;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 3 && sp == 0) {sp = i+1;}
        if (count == 4 && ep == 0) {ep = i-1;}
    }
    for (int i = sp; i != ep + 1; i++)
        str += buf[i];
    return str;
}

String getHum(char *buf)
{
    int count = 0, sp = 0;
    String str;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 4 && sp == 0) {sp = i+1;}
    }
    for (int i = sp; buf[i] != '$'; i++)
        str += buf[i];
    return str;
}

String getTime(char *buf)
{
    int count = 0, sp = 0, ep = 0;
    String str;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 1 && sp == 0) {sp = i+1;}
        if (count == 2 && ep == 0) {ep = i-1;}
    }
    for (int i = sp; i != ep + 1; i++)
        str += buf[i];
    str += String(F("%20"));
    sp =0; ep = 0; count = 0;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 2 && sp == 0) {sp = i+1;}
        if (count == 3 && ep == 0) {ep = i-1;}
    }
    for (int i = sp; i != ep + 1; i++)
        str += buf[i];
    str += String(F("&timezone=Australia%2FSydney"));
    return str;
}

void testWiFi()
{
    String data, stime;
    char raw[64] = "1,2015-02-27,01:44:33,38.5,66.6$";  
//    dtostrf(27.5, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    dtostrf(-3.0, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    Serial.println(F("testCaptureData"));
    int chk = DHT.read22(DHT22_PIN);
    float temp, hum;
    temp = DHT.temperature;
    hum = DHT.humidity;
    
    Serial.println(F("AT"));
    delay(2000);
    while (!Serial.find("OK")) {
        delay(2000);
        Serial.println("AT");
    }

    //Serial.println(F("connectWiFi"));
    if (connectWiFi()) {
        //stime = getTime(raw);
//GET　https://api.thingspeak.com/update?key=AVRDH90QKS69WV0D&field1=10&field2=40&created_at=2015-03-05%2011:19:00&timezone=Australia%2FSydney
        data = String(temp) + "&field2=" + String(hum);
        //     "&created_at=" + String(2015) + String(F("-")) + String(3) + String(F("-")) + String(5) +
        //     String(F("%20")) + String(1) + String(F(":")) + String(44) + String(F(":")) + String(45);            
        //data = String(temp) + String(F("&field2=")) + String(hum) + 
        //    String(F("&created_at=")) + String(stime);
        transmitData(data);
        delay(15000);  
    }
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void testMemSetup () {
    //Serial.begin(57600);
    Serial.println(F("\n[memCheck]"));
    Serial.println(freeRam());
}

void captureBlink() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(10);              // wait for a second
        digitalWrite(LED, LOW);    // turn the LED off by making the voltage LOW
        delay(400);             
    }
}

void uploadBlink() {
    for (int i = 0; i < 10; i++) {
        digitalWrite(LED, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(10);              // wait for a second
        digitalWrite(LED, LOW);    // turn the LED off by making the voltage LOW
        delay(120);             
    }
}

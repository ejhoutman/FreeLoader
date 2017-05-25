/*
  FreeLoader controller
  designed for an arduino Mega 2560


  Will work with any lcd with a Hitachi HD44780 driver
  Data bus lines 0-3 are not used in this mode
   LCD RS pin to digital pin 27
   LCD Enable pin to digital pin 26
   LCD D4 pin to digital pin 24
   LCD D5 pin to digital pin 25
   LCD D6 pin to digital pin 23
   LCD D7 pin to digital pin 22
   LCD R/W pin to ground
   LCD VSS pin to ground
   LCD VCC pin to 5V
   10k pot set up as a voltage divider with the wiper on the lcd contrast adjust pin


  CSV file is: force, position, miliseconds since start

*/

#include <Adafruit_ADS1015.h> //for the adc
#include <TimerOne.h>         //interupt timer
//#include <Q2HX711.h> // for the hx711 all in one loadcell amp/adc, not using anymore
#include <Encoder.h>         // for the position encoder
#include <Wire.h>           // i2c library
#include <LiquidCrystal.h>
#include <SPI.h>
//#include <SD.h> //sd card library. appears to be 4+ years out of date, using SdFat instead
#include "SdFat.h"
SdFat SD;



// set pin numbers
#define TopLimit 4        //top limit switch
#define STEP_pin 5        //GeckoDrive STEP pin
#define DIR_pin 6         //GeckoDrive DIR pin
//#define hx711_data_pin 7  //loadcell amp data pin
//#define hx711_clock_pin 8 //loadcell amp clock pin
#define chipSelect 53     //SD card CS pin, the SD also uses 51(MOSI), 50(MISO), and 52(SCK)
#define encoder1 2        //encoder pins 1 and 2, these must be interupt pins
#define encoder2 3        //on a mega the interupt pins are: 2,3,18,19,20,21
#define CPI 600           //number of lines per inch on the encoder strip
#define lcd_RS 27         //lcd Register select
#define lcd_En 26         //lcd Enable
#define lcd_D4 24         //lcd Data bus line 4
#define lcd_D5 25         //lcd Data bus line 5
#define lcd_D6 23         //lcd Data bus line 6
#define lcd_D7 22         //lcd Data bus line 7
#define led1 49           //front panel led 1
#define led2 48           //front panel led 2
#define led3 47           //front panel led 3
#define led4 46           //front panel led 4
#define button1 45        //front panel button 1
#define button2 44        //front panel button 2
#define button3 43        //front panel button 3
#define button4 42        //front panel button 4
//loadcell adc is on pins 20 and 21(i2c)
// define stuff
int oldPosition  = -999;
int newPosition = 0;
bool up = false;
bool waiting = true;
bool go = false;
unsigned long LastTime = 0;
char filename[15];
uint8_t i;
unsigned long RunTime;


Encoder myEnc(encoder1, encoder2); //set up the encoder
//Q2HX711 hx711(hx711_data_pin, hx711_clock_pin);
LiquidCrystal lcd(lcd_RS, lcd_En, lcd_D4, lcd_D5, lcd_D6, lcd_D7);//set up the lcd
Adafruit_ADS1115 ads;// set up the

void setup() {

  // setting up the controll pannel buttons and lights
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(button4, INPUT_PULLUP);
  digitalWrite(led1, LOW);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, LOW);
  digitalWrite(led4, LOW);
  //set up gecko drive and limit switch pins
  pinMode(STEP_pin, OUTPUT);
  pinMode(DIR_pin, OUTPUT);
  pinMode(TopLimit, INPUT_PULLUP);
  digitalWrite(STEP_pin, LOW);
  digitalWrite(DIR_pin, HIGH);

  Timer1.initialize(500);
  Timer1.attachInterrupt(Step); //run step every 500 μs
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));

  Serial.begin(115200);
  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.0078125mV


  lcd.begin(16, 2);


  lcd.print("Initializing SD...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card failed");
    lcd.setCursor(0, 1);
    lcd.print("or not present.");
    // don't do anything more if sd is not working
    while (1) {}
  }

  lcd.setCursor(0, 1);
  lcd.print("SD initialized.");

  // contruct a unique filename

  strcpy(filename, "FRELDR00.CSV");
  for (i = 0; i < 100; i++) { //go around in this loop until we find a filename that doesn't exist

    filename[6] = '0' + i / 10;
    filename[7] = '0' + i % 10;
    filename[12] = 0;          //null terminate

    // break when we find a filename that is not already used
    if (!SD.exists(filename)) {
      break;
    }
  }
  delay(1000);
  RunTime = millis();
}



void Step() { //toggle the geckodrive STEP pin every 500 μs
  if (go == true) {
    digitalWrite(STEP_pin, !digitalRead(STEP_pin));

  }
}

void loop() {
  newPosition = myEnc.read();
  unsigned long Time = millis();


  /*
    if (newPosition != oldPosition) {
     oldPosition = newPosition;
    }
  */

  if (up == false && Time > LastTime + 300) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(ads.readADC_SingleEnded(0));
    lcd.setCursor(0, 1);
    //Serial.print(", ");
    lcd.print(newPosition);
    LastTime = Time;
  }


  if (digitalRead(button1) == HIGH && up == false) { // reset when the reset button is pressed
    up = true;
    go = false;
    digitalWrite(led3, LOW);
    digitalWrite(DIR_pin, LOW);
    digitalWrite(led1, HIGH);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Reseting");
  }


  if (digitalRead(TopLimit) == LOW && up == true) {
    //    stop when the limit switch is reached
    noInterrupts();
    digitalWrite(DIR_pin, HIGH);
    digitalWrite(led1, LOW);
    up = false;
    newPosition = 0;
    strcpy(filename, "FRELDR00.CSV");
    for (i = 0; i < 100; i++) {  //go around in this loop until we find a filename that doesn't exist

      filename[6] = '0' + i / 10;
      filename[7] = '0' + i % 10;
      filename[12] = 0;          //null terminate

      // break when we find a filename that is not already used
      if (!SD.exists(filename)) {
        break;
      }
    }
    RunTime = millis();
    waiting = true;
    digitalWrite(led2, HIGH);
  }


  if (waiting == true && digitalRead(button2) == HIGH) { // if the start button is pressed start moving the stepper
    digitalWrite(led2, LOW);
    digitalWrite(led3, HIGH);
    waiting = false;
    go = true;
    interrupts();
  }


  if (up == false && waiting == false) { // colect data and write to sd
    String dataString = "";
    dataString += String(ads.readADC_SingleEnded(0));
    dataString += ", ";
    dataString += String(newPosition);
    dataString += ", ";
    dataString += String(millis() - RunTime);
    File dataFile = SD.open(filename, FILE_WRITE);

    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("error opening");
      lcd.setCursor(0, 1);
      lcd.print(filename);
    }

  }

}

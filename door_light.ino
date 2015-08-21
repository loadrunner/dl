#include <VirtualWire.h>
#include <RCSwitch.h>
#include <Wire.h>
#include <RTClib.h>
#include <TM1636.h>

#define LIGHT_SENSOR_PIN A1 //(old 6)
#define LIGHT_PIN 13 //old
#define RX_PIN 0 //INTERRUPT 0, PIN 2 //old
#define TX_PIN 5 //old
#define RED_PIN 10 //old
#define GREEN_PIN 11 //old
#define BLUE_PIN 12 //old
#define PIEZO_PIN 6
#define DOOR_PIN 9 //tmp, btn
#define TEMPERATURE_PIN A0
#define BTN_1_PIN 9
#define BTN_2_PIN 10
#define BTN_3_PIN 11

#define LOOP_DELAY 1000
#define MINI_LOOP_DELAY 50
#define AVG_SAMPLES 60
#define RGB_FADE_SPEED 5     // make this higher to slow down

RCSwitch mySwitch = RCSwitch();
RTC_DS1307 RTC;
TM1636 tm1636(7,8);

int loopTimer;

byte lightMode;
int lightLimit;
float lightAvg;
boolean lightOn;
long lightTimeOn;

byte rgbMode;
boolean rLightOn;
boolean gLightOn;
boolean bLightOn;

struct 
{
  int8_t numbers[4];
  bool points;
} displayData;

int readLightLevel()
{
  /*
  int photocellReading = 0;//analogRead(SENSOR_PIN);
  //Serial.print("Analog reading = ");
  //Serial.println(photocellReading); // the raw analog reading
  //Serial.println(photocellReading*0.0049f); // volts
  
  return photocellReading;
  */
  int sensorValue = analogRead(LIGHT_SENSOR_PIN);
  return (float)(1023 - sensorValue) * 10 / sensorValue;
}

int getLightLimit()
{
  return lightLimit + (lightOn ? 30 : 0);
}

void setLight(boolean on)
{
  digitalWrite(LIGHT_PIN, on);
  lightOn = on;
}

void readCommand()
{
  long value = mySwitch.getReceivedValue();
  mySwitch.resetAvailable();
  
  Serial.print("Got value: ");
  Serial.println(value);
  
  int id = value & 15;
  int security = (value >> 20) & 15;
  
  if (security == 12 && id == 1)
  {
    int command = (value >> 4) & 15;
    int param = (value >> 8) & 4095;
    Serial.print("Running command: ");
    Serial.print(command);
    Serial.print(" with param ");
    Serial.println(param);
    
    switch (command)
    {
      case 1: //ON
        Serial.println("Chaning mode to ON");
        lightMode = 1;
        break;
      case 2: //OFF
        Serial.println("Chaning mode to OFF");
        lightTimeOn = 0;
        lightMode = 0;
        break;
      case 3: //AUTO
        Serial.print("Chaning mode to AUTO with light limit ");
        lightTimeOn = 0;
        lightMode = 2;
        if (param >= 0 && param <= 1024)
          lightLimit = param;
        else
          lightLimit = 10;
        Serial.println(lightLimit);
        break;
      case 4: //TIMED
        Serial.print("Chaning mode to TIMED with param ");
        Serial.print(param);
        lightTimeOn = (long) 60 * param;
        lightMode = 3;
        Serial.print(", seconds ");
        Serial.println(lightTimeOn);
        break;
      case 5:
        Serial.println("Changing RGS mode to on.");
        rgbMode = 1;
        break;
      case 6:
        Serial.println("Changing RGS mode to off.");
        rgbMode = 0;
        break;
      case 7:
        Serial.println("Changing RGS mode to fade.");
        rgbMode = 2;
        break;
    }
  }
  else
    Serial.println("Security code or id is invalid!");
}

float readTemperature()
{
  int a = analogRead(TEMPERATURE_PIN);
  int B = 3975;
  float resistance   = (float)(1023-a)*10000/a; 
  return 1/(log(resistance/10000)/B+1/298.15)-273.15;
}

void loopLight()
{
  Serial.print("light is ");
  Serial.print(lightOn ? "on. " : "off. ");
  
  float light;
  
  switch (lightMode)
  {
    case 1:
      if (!lightOn)
      {
        Serial.print("light mode ON, turning on.");
        setLight(true);
      }
      break;
    case 2:
      Serial.print("light mode AUTO .. ");
      light = readLightLevel();
      lightAvg = (lightAvg * (AVG_SAMPLES - 1) + light) / AVG_SAMPLES;
      Serial.print(" level is ");
      Serial.print(light);
      Serial.print(", avg is ");
      Serial.print(lightAvg);
      Serial.print(", limit is ");
      Serial.print(getLightLimit());
      
      if (lightAvg >= getLightLimit() && lightOn)
      {
        Serial.print(" turinng led off.");
        setLight(false);
        //reset avg in order to prevent flickers
        lightAvg = 1024;
      }
      else if (lightAvg < getLightLimit() && !lightOn)
      {
        Serial.print(" turinng led on.");
        setLight(true);
        //reset avg in order to prevent flickers
        lightAvg = 0;
      }
      break;
    case 3:
      Serial.print("light mode TIMED .. ");
      if (lightTimeOn > 0)
      {
        lightTimeOn -= LOOP_DELAY / 1000.0f;//TODO: rewrite this part to use real time clock
        if (!lightOn)
        {
          setLight(true);
          Serial.print(" turning on.");
        }
      }
      else if (lightTimeOn <= 0 && lightOn)
      {
        setLight(false);
        lightMode = 0;
        Serial.print(" turning off.");
      }
      break;
    case 0:
    default:
      if (lightOn)
      {
        Serial.print("light mode OFF, turning off.");
        setLight(false);
      }
  }
  
  Serial.print("\n");
}

void loopRGB()
{
  switch (rgbMode)
  {
    case 1:
      if (!rLightOn)
      {
        Serial.println("rgb mode ON, turning on red.");
        digitalWrite(RED_PIN, HIGH);
        rLightOn = true;
      }
      if (!gLightOn)
      {
        Serial.println("rgb mode ON, turning on green.");
        digitalWrite(GREEN_PIN, HIGH);
        gLightOn = true;
      }
      if (!bLightOn)
      {
        Serial.println("rgb mode ON, turning on blue.");
        digitalWrite(BLUE_PIN, HIGH);
        bLightOn = true;
      }
      break;
    case 2:
      rLightOn = true;
      gLightOn = true;
      bLightOn = true;
      int r, g, b;
     
      // fade from blue to violet
      for (r = 0; r < 256; r++) { 
        analogWrite(RED_PIN, r);
        delay(RGB_FADE_SPEED);
      } 
      // fade from violet to red
      for (b = 255; b > 0; b--) {
        analogWrite(BLUE_PIN, b);
        delay(RGB_FADE_SPEED);
      }
      // fade from red to yellow
      for (g = 0; g < 256; g++) {
        analogWrite(GREEN_PIN, g);
        delay(RGB_FADE_SPEED);
      }
      // fade from yellow to green
      for (r = 255; r > 0; r--) {
        analogWrite(RED_PIN, r);
        delay(RGB_FADE_SPEED);
      }
      // fade from green to teal
      for (b = 0; b < 256; b++) {
        analogWrite(BLUE_PIN, b);
        delay(RGB_FADE_SPEED);
      }
      // fade from teal to blue
      for (g = 255; g > 0; g--) {
        analogWrite(GREEN_PIN, g);
        delay(RGB_FADE_SPEED);
      }
      
      break;
    case 0:
    default:
      if (rLightOn)
      {
        Serial.print("rgb mode ON, turning off red.");
        digitalWrite(RED_PIN, LOW);
        rLightOn = false;
      }
      if (gLightOn)
      {
        Serial.print("rgb mode ON, turning off green.");
        digitalWrite(GREEN_PIN, LOW);
        gLightOn = false;
      }
      if (bLightOn)
      {
        Serial.print("rgb mode ON, turning off blue.");
        digitalWrite(BLUE_PIN, LOW);
        bLightOn = false;
      }
  }
}

void loopSensors()
{
  boolean doorOpen = digitalRead(DOOR_PIN);
  
  Serial.print("door 1 is ");
  Serial.println(doorOpen ? "open" : "closed");
  
  char *msg;
  
  if (doorOpen)
  {
    analogWrite(PIEZO_PIN, 15);
    delay(50);
    digitalWrite(PIEZO_PIN, LOW);
    //msg = "door is open!";
  }
  //else
  //{
  //  msg = "door is closed!";
  //}
  
  //vw_send((uint8_t *)msg, strlen(msg));
  //vw_wait_tx();
}

void loopClock()
{
  DateTime now = RTC.now();
  /*
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  */
  displayData.numbers[0] = now.hour()/10;
  displayData.numbers[1] = now.hour()%10;
  displayData.numbers[2] = now.minute()/10;
  displayData.numbers[3] = now.minute()%10;
  displayData.points = (now.second() % 2 == 0);
}

void loopTemperature()
{
  boolean btnPressed = !digitalRead(BTN_1_PIN);
  if (btnPressed)
  {
    int temp = readTemperature();
    //Serial.print("temp:");
    //Serial.println(temp);
    if(temp < 0)
    {
      displayData.numbers[0] = INDEX_NEGATIVE_SIGH;
      temp = abs(temp);
    }
    else if(temp < 100)
      displayData.numbers[0] = INDEX_BLANK;
    else
      displayData.numbers[0] = temp / 100 % 10;
    
    displayData.numbers[1] = temp / 10 % 10;
    displayData.numbers[2] = temp % 10;
    displayData.numbers[3] = 12; //index of 'C' for celsius degree symbol
    
    displayData.points = false;
  }
}

void loopLightLevel()
{
  boolean btnPressed = !digitalRead(BTN_2_PIN);
  if (btnPressed)
  {
    int light = readLightLevel();
    Serial.print("light:");
    Serial.println(light);
    if(light < 1000)
      displayData.numbers[0] = INDEX_BLANK;
    else
      displayData.numbers[0] = light / 1000 % 10;
    
    if(light < 100)
      displayData.numbers[1] = INDEX_BLANK;
    else
      displayData.numbers[1] = light / 100 % 10;
    
    displayData.numbers[2] = light / 10 % 10;
    displayData.numbers[3] = light % 10;
    
    displayData.points = false;
  }
}

void loopScreen()
{
  tm1636.display((int8_t*) displayData.numbers);
  tm1636.point(displayData.points);
}

void setup()
{
  Serial.begin(9600);
  Wire.begin();
  RTC.begin();
  //RTC.adjust(DateTime(__DATE__, __TIME__)); //set time with compilation time
  
  tm1636.set(BRIGHT_TYPICAL);
  tm1636.init();
  
  //pinMode(TX_PIN, OUTPUT);
  //digitalWrite(TX_PIN, LOW);
  
  pinMode(BTN_1_PIN, INPUT_PULLUP);
  pinMode(BTN_2_PIN, INPUT_PULLUP);
  pinMode(BTN_3_PIN, INPUT_PULLUP);
  
  pinMode(PIEZO_PIN, OUTPUT);
  digitalWrite(PIEZO_PIN, LOW);
  
  //pinMode(DOOR_PIN, INPUT_PULLUP);
  /*
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  
  pinMode(RED_PIN, OUTPUT);
  digitalWrite(RED_PIN, LOW);
  pinMode(GREEN_PIN, OUTPUT);
  digitalWrite(GREEN_PIN, LOW);
  pinMode(BLUE_PIN, OUTPUT);
  digitalWrite(BLUE_PIN, LOW);
  
  mySwitch.enableReceive(RX_PIN);
  */
  //vw_set_ptt_inverted(true);
  //vw_set_rx_pin(4);
  //vw_set_tx_pin(TX_PIN);
  //vw_setup(2000);// speed of data transfer Kbps

  loopTimer = LOOP_DELAY;
  
  lightMode = 0;
  lightLimit = 10;
  lightAvg = 1024;
  lightTimeOn = 0;
  lightOn = false;
  
  rgbMode = 0;
  rLightOn = false;
  gLightOn = false;
  bLightOn = false;
}

void loop()
{
  if (loopTimer >= LOOP_DELAY)
  {
    loopTimer = 0;
    
//  if (mySwitch.available())
//  {
//    readCommand();
//  }
    
//  loopLight();
//  loopRGB();
//  loopSensors();
    loopClock();
  }
  
  loopTemperature();
  loopLightLevel();
  loopScreen();
  
  delay(MINI_LOOP_DELAY);
  loopTimer += MINI_LOOP_DELAY;
}


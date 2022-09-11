/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG
// #define BLYNK_HEARTBEAT 40

// #define BLYNK_NO_BUILTIN   // Disable built-in analog & digital pin operations
//#define BLYNK_NO_FLOAT     // Disable float operations

/* Fill-in your Template ID (only if using Blynk.Cloud) */
#define BLYNK_TEMPLATE_ID "TMPLRl0-o2TC"
#define BLYNK_DEVICE_NAME "Farm Project"

#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <TimeLib.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "IZfV43gICj1UJmLJmq1bV9tQFSkLtZGI";

// Your WiFi credentials.
// Set password to "" for open networks.
// char ssid[] = "Connectify-me";
// char pass[] = "lifareb0";
char ssid[] = "DESKTOPK46";
char pass[] = "12345678";

// #define EspSerial Serial1
// or Software Serial on Uno, Nano...
#include <SoftwareSerial.h>
SoftwareSerial EspSerial(10, 11); // RX, TX
// SoftwareSerial EspSerial(9, 10); // RX, TX

// Your ESP8266 baud rate:
#define ESP8266_BAUD 9600

BlynkTimer timer;
// WidgetRTC rtc; //For the time stuff

ESP8266 wifi(&EspSerial);

// On_Off values
#define Blynk_ON 1 // Value sent to Blynk server pin
#define Blynk_OFF 0
#define Relay_ON HIGH // Digital Write value for relay
#define Relay_OFF LOW

// Pins
#define NPKsensorPin A0

#define SoilMoistureSensPin A2
#define NutrientValve 5
#define WaterValve 3

// Blynk virtual pins
#define out_waterSensorVPin V6
#define out_nutrientSensorVPin V1
#define in_overrideWaterVPin V2
#define in_overrideNutrVPin V3
#define in_WaterSwitchVPin V4
#define in_NutrSwitchVPin V5
#define weeksElapsed V7

// Thresholds and values
#define N_highThresh 30 // 80
#define N_lowThresh 20     // 55
#define Moist_highThresh 70
#define Moist_lowThresh 20
#define waterFlushTime 3000 // in msecs

#define plantingWeek 1635434979 // 28/10/2021

#define moistZeroValue 499
#define moistHundredValue 197
#define moistDryValue 414
#define moistRlyWetValue 290
#define nZeroValue 0
#define nHundredvalue 1024

// Blynk vPin values
bool nutrOverride_val = false;
bool waterOverride_val = false;
bool waterSwitch_val = false;
bool nutrSwitch_val = false;

//      INITIALISING
int currentWaterLevel, currentNutrientLevel, sentNitr,sumNitr,countNitr, maxNitr = 0;
int currentHour, elapsedWeeksSincePlanting = 0;
int soilMoistureValue;

// After every nitrogen application, water
// should be applied through the pipe for some secs.
// The flag shows if a flush is pending
bool waterFlushPending = false; 
unsigned long previousMillisFlush;
bool timerStartFlush = false;



//***************************
//          SETUP
//***************************
void setup()
{
  // Debug console
  Serial.begin(9600);
  // analogReference(INTERNAL);
  pinMode(NutrientValve, OUTPUT);
  pinMode(WaterValve, OUTPUT);
  pinMode(SoilMoistureSensPin, INPUT);

  delay(10);

  // Set ESP8266 baud rate
  EspSerial.begin(ESP8266_BAUD);
  delay(10);

  Blynk.begin(auth, wifi, ssid, pass);
  //  while(!Blynk.connect()){} //Wait until connected

  timer.setInterval(500L, nitrogen_read_value);
  timer.setInterval(2000L, moisture_read_value);
  timer.setInterval(1000L, nutrientSwitching);
  timer.setInterval(1000L, waterSwitching);
  timer.setInterval(5000L, sendSensor);
  timer.setInterval(10000L, timerUpdate);
  timer.setInterval(10000L, updateWeeks);
  timer.setInterval(1000L, serialPrintValues);

  // Initialising the relays to be off
  digitalWrite(WaterValve, LOW);
  digitalWrite(NutrientValve, LOW);
}



//***************************************
//////////////////////////////////////
//              LOOP
/////////////////////////////////////

void loop()
{

  Blynk.run();
  // Setup a function to be called every second
  timer.run();
  
}


//***************************************
//        TIMER UPDATE
//***************************************
void timerUpdate()
{
  Serial.println("Timer update");
  Serial.println("Timer update");
  Blynk.sendInternal("rtc", "sync");

  //Update weeks elapsed

}

//***************************************
//        NUTRIENT SWITCHING
//***************************************
void nutrientSwitching()
{
  if (nutrOverride_val == false)
  {
    if (currentNutrientLevel > N_highThresh ||
        !(currentHour == 5 || currentHour == 6)) // Switch off nutrient
    {
      digitalWrite(NutrientValve, Relay_OFF); // Nutrient
      nutrSwitch_val = false;
      Blynk.virtualWrite(in_NutrSwitchVPin, Blynk_OFF);
      nutrSwitch_val = false;
      Serial.println("N Off");
    }
    else if (currentNutrientLevel <= N_lowThresh &&
             (currentHour == 5 || currentHour == 6))
    {
      // Switch on Nutrient
      digitalWrite(NutrientValve, Relay_ON);
      nutrSwitch_val = true;
      if (waterFlushPending != true)
        waterFlushPending = true;
      Blynk.virtualWrite(in_NutrSwitchVPin, Blynk_ON);
      nutrSwitch_val = true;
      // Serial.println("N On");
    }
  }
  else
    digitalWrite(NutrientValve, nutrSwitch_val ? Relay_ON : Relay_OFF);
}
//***************************************
//        WATER SWITCHING
//***************************************
/*There is a potential problem with this flushing issue*/

void waterSwitching()
{
  if (waterFlushPending)
  {
    // Only come on after the nutrient is done
    if(digitalRead(NutrientValve)){
      return;
    }

    if (!timerStartFlush) // If timer has not started start timer
    {
      previousMillisFlush = millis();
      timerStartFlush = true;
      digitalWrite(WaterValve, HIGH);
      Blynk.virtualWrite(in_WaterSwitchVPin, Blynk_ON);
      waterSwitch_val = true;
    }
    else
    {
      if (millis() - previousMillisFlush < waterFlushTime)
      { // If the number of seconds have not elapsed stop here
        return;
      }
      else
      {
        timerStartFlush = false;
        waterFlushPending = false;
      } // Else stop timer and pending and move on
    }
  }
  else
  {
    if (waterOverride_val != true) // For Water
    {
      if (currentWaterLevel > Moist_highThresh ||
          !(currentHour == 5 || currentHour == 6)) // Switch off water
      {
        digitalWrite(WaterValve, Relay_OFF);
        Blynk.virtualWrite(in_WaterSwitchVPin, Blynk_OFF);
        waterSwitch_val = false;
      }
      else if (currentWaterLevel >= Moist_lowThresh &&
               (currentHour == 5 || currentHour == 6)) // Switch on water
      {
        digitalWrite(WaterValve, Relay_ON);
        Blynk.virtualWrite(in_WaterSwitchVPin, Blynk_ON);
        waterSwitch_val = true;
        // Serial.println("Water v on");
      }
    }
    else
      digitalWrite(WaterValve, waterSwitch_val ? Relay_ON : Relay_OFF);
  }
}

//***************************************
//          BLYNK_CONNECTED
//***************************************
// Initialising on Connection
BLYNK_CONNECTED()
{
  Blynk.sendInternal("rtc", "sync"); // Request current local time
  // currentHour = hour();
  Blynk.virtualWrite(in_overrideNutrVPin, Blynk_OFF);
  Blynk.virtualWrite(in_overrideWaterVPin, Blynk_OFF);
  Blynk.virtualWrite(in_NutrSwitchVPin, Blynk_OFF);
  Blynk.virtualWrite(in_WaterSwitchVPin, Blynk_OFF);
}

//***************************************
///////////////////////////////////////////////
//            Virtual Pin updates
///////////////////////////////////////////////

BLYNK_WRITE(V3)
{
  nutrOverride_val = param.asInt() == 1;
  Serial.print("Nutr Overr: ");
  Serial.println(nutrOverride_val);
}
BLYNK_WRITE(V2)
{
  waterOverride_val = param.asInt() == 1;
  Serial.print("Water Overr: ");
  Serial.println(waterOverride_val);
}
BLYNK_WRITE(V5)
{
  nutrSwitch_val = param.asInt() == 1;
  Serial.print("Nutr sw value: ");
  Serial.println(nutrSwitch_val);
}
BLYNK_WRITE(V4)
{
  waterSwitch_val = param.asInt() == 1;
  Serial.print("Water sw value: ");
  Serial.println(waterSwitch_val);
}

BLYNK_WRITE(InternalPinRTC)
{                          // check the value of InternalPinRTC
  long t = param.asLong(); // store time in t variable
  setTime(t);
  currentHour = hour();
  Serial.print("Hour: ");
  Serial.println(currentHour);

  elapsedWeeksSincePlanting = round((float)(now()-plantingWeek)/7/24/60/60);  
}

///////////////////////////////////////////////
//           UPDATE PLANTING WEEKS
///////////////////////////////////////////////
void updateWeeks(){
  Blynk.virtualWrite(weeksElapsed, elapsedWeeksSincePlanting);
}


//***************************************
///////////////////////////////////////////////
//        MOISTURE READ VALUE
///////////////////////////////////////////////
//***************************************
// Read sensor functions
void moisture_read_value()
{
  soilMoistureValue = analogRead(SoilMoistureSensPin); // put Sensor insert into soil
  int soilmoisturepercent = map(soilMoistureValue, moistDryValue,
                                moistRlyWetValue, 18, 48);
  currentWaterLevel = soilmoisturepercent;
}

///////////////////////////////////////////////
//          NITROGEN READ VALUE
///////////////////////////////////////////////

void nitrogen_read_value()
{
  int newNitrogenValue = analogRead(NPKsensorPin);
  currentNutrientLevel = map(newNitrogenValue, nZeroValue,
                              nHundredvalue, 0, 1999);
  
  //Add values and count updates
//  if(currentNutrientLevel != 0){sumNitr = sumNitr + currentNutrientLevel;
//  countNitr = countNitr + 1;}

//  Serial.print("Cu "); Serial.println(currentNutrientLevel);
//  Serial.print("Sum "); Serial.println(sumNitr);
//  Serial.print("Cnt "); Serial.println(countNitr);
  // Replace value if greater than (because sensor values are flunctuating)
   if (maxNitr < currentNutrientLevel)
     maxNitr = currentNutrientLevel;
//   Serial.print("Ma "); Serial.println(maxNitr);
    //else do nothing
}

///////////////////////////////////////////////
//          PRINT VALUES TO SERIAL
///////////////////////////////////////////////

void serialPrintValues()
{

  Serial.print("W: ");
  Serial.println(soilMoistureValue);
//  Serial.println(currentWaterLevel);
  Serial.print("N: ");
  Serial.println(sentNitr);

  // Serial.print("N map");
  // Serial.println(currentNutrientLevel);
  
}
//*********************************************
//        SEND SENSOR
//***************************************
// This sends the sensor values to the Virtual pins and reads the value
// If override is enabled, the switch works
void sendSensor()
{
//   Serial.println(currentNutrientLevel);

  // upload the sensor values online
  Blynk.virtualWrite(out_waterSensorVPin, currentWaterLevel);
  sentNitr = maxNitr;//sumNitr/countNitr;//
  Blynk.virtualWrite(out_nutrientSensorVPin, sentNitr); //send mean
  //reset nutr level
  sumNitr = 0;
  countNitr = 0;
  maxNitr = 0;  
}

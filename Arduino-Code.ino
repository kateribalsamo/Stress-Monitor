//Includes necessary libraries 
#include <OneWire.h> //To communicate with one wire device (temperature sensor) 
#include <DallasTemperature.h> //Facilitate temperature sensor interaction 
#include <Wire.h> //I2C communciation (heart rate, OLED, and accelerometer)
#include "MAX30105.h" //Pulse oximeter 
#include "heartRate.h" //Heart rate calculations 
#include <Adafruit_SSD1306.h> //OLED display
#include <Adafruit_GFX.h> //OLED graphics 

//Defines digital pin for temperature sensor 
#define ONE_WIRE_BUS 4 //GPIO4 ESP32

//Setup OneWire communication 
OneWire oneWire(ONE_WIRE_BUS);

//Pass communciation to Dallas temperature sensor 
DallasTemperature sensors(&oneWire); 

//temperature buffers for data acquisition 
#define MAX_SAMPLES 30 //One sample per second for 3 minutes. 30 SECONDS FOR DEBUGGING
float tempBuffer[MAX_SAMPLES]; //Temp buffer 
int sampleIndexT=0; //Current index in buffer 
unsigned long lastSampleTime = 0; //For timing the samples
const unsigned long sampleInterval = 1000; //Time between samples (1s) 
bool tempBufferSent = false; //flag

//Defining pulse oximeter sensor as a class
MAX30105 particleSensor; 

//Pulse oximeter sensor setting values 
const byte ledBrightness = 50;
const byte sampleAverage = 1; 
const byte ledMode =2; 
const byte sampleRate = 100; 
const int pulseWidth = 69; 
const int adcRange = 4096; 

//Defining OLED screen parameters
#define MAX_BRIGHTNESS 255
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 01
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//OLED variables for updating display after each measurement buffer is filled 
bool tempDone = false; 
bool heartDone = false; 
bool hrvDone = false; 
bool sendingToApp = false;

//OLED update display function 
void updateStatusDisplay() 
{
  display.clearDisplay(); 
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE); 

  if (sendingToApp)
  {
    display.setCursor(0,0); //If all values collected will show its sending to app 
    display.println("Sending to App...");
  }
  else
  {
    display.setCursor(0,0); //If values not all gathered yet will show measuring
    display.println("Measuring..."); 
  }

  if (tempDone)  //Shows T when temp is done measuring 
  {
    display.setCursor(SCREEN_WIDTH - 30, 0);
    display.print("T"); 
  }

  if (heartDone) //Shows H when heart is done measuring 
  {
    display.setCursor(SCREEN_WIDTH - 20, 0); 
    display.print("H");
  }

  if (hrvDone) 
  {
    display.setCursor(SCREEN_WIDTH - 10, 0); 
    display.print("V");
  }
  display.display(); //Updates to display 
}

//Pulse oximeter buffers for data acquisition 
#define MAX_HRV_SAMPLES 100 //For HRV only as it needs to be detected each beat 
float heartRateBuffer[MAX_SAMPLES]; //Heart rate buffer in BPM 
float hrvBuffer[MAX_HRV_SAMPLES]; //Heart rate variability buffer (ms)
float currentBPM=0; //Current heart rate 
unsigned long lastBeatTime=0; //Calculates last time heart beated 
unsigned long lastHRSampleTime = 0; //Calculates last time heart rate was sampled 
float rrInterval = 0; //current HRV interval 
int sampleIndexHR=0; //Heart buffer index
int sampleIndexHRV=0; //HRV buffer index 
bool hrBufferSent = false; //flag
bool hrvBufferSent = false; //flag
float sumSqDiffs = 0; //RMSSD calculation 
float rmssd = 0; //RMSSD value 
bool hrvCalculatedSent = false; 


void setup()
{
  //Begin serial communication 
  Serial.begin(115200); //Baud rate of 115200 
  Serial.print("Serial monitor ready");
  
  //Start up temp library 
  sensors.begin(); 

  //Begin I2C communciation 
  Wire.begin(21, 22);

  //Initializing sensor 
  if (!particleSensor.begin (Wire, I2C_SPEED_FAST)) //Checks if sensor is detected and sets it to 400 kHz
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while(1); //will not continue forward while it continues 
  }

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configures settings 

  //Initializing OLED 
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) //If OLED was not found 
  {
    Serial.println(F("OLED was not found. Please check wiring and power.")); 
    while (1);
  }
  
  //OLED initial parameters 
  display.clearDisplay(); // Clears display
  display.setTextSize(1); //Text size
  display.setTextColor(SSD1306_WHITE); //Text color 

  //Initial message placement centered 
  String message = "Initializing..."; 
  int x = (SCREEN_WIDTH - message.length() *6) / 2; //Width of message on screen 
  int y = (SCREEN_HEIGHT - 8) / 2; //Height of message on screen 
  display.setCursor(x,y); 
  display.print(message); 
  display.display(); 
  delay(1000); 
  Serial.print(message);
}

void loop()
{
  Serial.println("Loop");
  unsigned long currentMillis = millis(); //Update run time
  //Acquiring sample data for the buffers 
  if (sampleIndexT < MAX_SAMPLES && currentMillis - lastSampleTime >=sampleInterval) //If maximum samples have not been reahced yet and the sample interval has surpassed we continue gathering samples 
  {
    lastSampleTime = millis(); //Updates sample time 

    //Temp buffer
    sensors.requestTemperatures(); //Temperature sensor does its thing 
    float temp =sensors.getTempFByIndex(0); //Reads current temp on sensor and stores it in variable "temp" in Farenheit
    tempBuffer[sampleIndexT++] = temp; //Puts new temp value in buffer 
  }

  //Acquiring heart rate and heart rate variability sample data 
  long irValue=particleSensor.getIR(); //Retrieves IR value from sensor 
  if(checkForBeat(irValue) ==true) //If a beat has been detected
  {
    rrInterval =(millis() - lastBeatTime) ; //Time between beats (ms)
    lastBeatTime=millis(); //Updates last beat time
    currentBPM= 60000 / rrInterval; //Current BPM 

    if(currentBPM >=40 && currentBPM<=200)  //Ensures beat detected is valid 
    {
      if(sampleIndexHR < MAX_SAMPLES && (millis()-lastHRSampleTime) >=sampleInterval)
      {
        lastHRSampleTime = millis(); 
        heartRateBuffer[sampleIndexHR++] = currentBPM; //Adds valid BPM to buffer
        Serial.print(currentBPM);
      }

      if(sampleIndexHRV< MAX_HRV_SAMPLES)
      {
        hrvBuffer[sampleIndexHRV++] = rrInterval; //Adds valid rr to buffer 
        Serial.print(rrInterval);
      }
    }
  }

  //Runs and prints temp buffer to serial monitor for debugging

  if(sampleIndexT == MAX_SAMPLES && !tempBufferSent) // If max samples have been reached for temp and it has not yet been outputted into serial monitor 
  {
    Serial.println("\n Temperature Buffer Complete."); //Prints to serial monitor
    for (int i=0; i<MAX_SAMPLES; i++) //Goes through entire buffer
    {
      Serial.print("Sample ");
      Serial.print(i);
      Serial.print(":  ");
      Serial.print(tempBuffer[i]);
      Serial.println(" °F");
    }
    tempBufferSent = true; //Updates flag 
    tempDone = true; //Updates flag 
    updateStatusDisplay(); //Runs function 
  }

  if(sampleIndexHR == MAX_SAMPLES && !hrBufferSent) //If max samples have been reached for heart rate and it has not yet been outputted into serial monitor 
  {
    Serial.println("\n Heart Rate Buffer Complete."); //Prints to serial monitor 
    for (int i=0; i<MAX_SAMPLES; i++)
    {
      Serial.print("Sample ");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(heartRateBuffer[i]);
      Serial.println(" BPM"); 
    }
    hrBufferSent = true; //Updates flag 
    heartDone = true; //Updates flag
    updateStatusDisplay(); //Runs function 
    delay(5);
  }

  if(sampleIndexHRV == MAX_HRV_SAMPLES && !hrvBufferSent) //If max samples have been reached for HRV and buffer has not yet been printed to serial monitor. 
  {
    Serial.println("\n RR Interval Buffer Complete."); //Prints to serial monitor 
    for (int i=0; i<MAX_HRV_SAMPLES; i++)
    {
      Serial.print("Sample ");
      Serial.print(i); 
      Serial.print(": ");
      Serial.print(hrvBuffer[i]);
      Serial.println(" ms"); 
    }
    hrvBufferSent = true; //Updates flag 
    hrvDone = true; //Updates flag
    updateStatusDisplay(); //Runs function 
    delay(5); 
  }

  //HRV calculation using RMSSD for short term beat to beat variability
  if(hrvBufferSent && !hrvCalculatedSent) //Runs when HRV buffer is filled and only calculates once 
  {
    for (int i=1; i < MAX_HRV_SAMPLES; i++)
    {
      float diff = hrvBuffer[i] - hrvBuffer[i-1]; //Difference between last two index values in the buffer 
      sumSqDiffs += diff * diff; //Adds square of the difference between two consecutive RR intervals
    }
  rmssd = sqrt(sumSqDiffs / (MAX_HRV_SAMPLES - 1));
  Serial.print("Calculated HRV (RMSSD): ");
  Serial.print(rmssd);
  Serial.println(" ms"); 
  hrvCalculatedSent = true; 
  }

  if(tempDone == true && heartDone == true && hrvDone == true && !sendingToApp) //If values are done collecting
  {
    sendingToApp = true; //Flag
    updateStatusDisplay(); //Functions 
  }
  delay(100); 

}

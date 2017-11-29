/*
 * The MIT License (MIT) Copyright (c) 2017 by David Bird.
 * 
 * A system that uses an ESP8266 and MAX30100 integrated pulse oximetry and heart-rate sensor. It combines two LEDs, a photodetector, 
 * optimised optics, and low-noise analog signal processing to detect pulse oximetry and heart-rate signals. The result is a display 
 * of a users blood oxygen levels and pulse rate.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files 
 * (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, 
 * publish, distribute, but not to use it commercially for the purposes of profit making or to sub-license and/or to sell copies of 
 * the Software or to permit persons to whom the Software is furnished to do so, subject to the following conditions:  
 *  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 * See more at http://dsbird.org.uk 
 * 
 * SPARKFUN Code is released under the [MIT License](http://opensource.org/licenses/MIT).
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 * Copyright (C) 2017 Adafruit, All Rights Reserved.
 * 
 * Hardware Connections (Breakoutboard or TFT Display to ESP8266):  
 * Most MAX30105 Breakout Boards have a voltage regulator and can use either 5V or 3.3V, however 5V is ideal.
 * -MAX30105 5V  = 5V (or 3.3V)  
 * -MAX30105 GND = GND  
 * -MAX30105 SDA = D2  
 * -MAX30105 SCL = D1
 * 
 * Display is ILI9341 2.2", 2.4" or 2.8" TFT
 * -ILI9341 CS   = D8
 * -ILI9341 MOSI = D7
 * -ILI9341 D/C  = D3 
 * -ILI9341 RST  = D4
 * -ILI9341 SCK  = D5
 * -ILI9341 Vcc  = 5V
 * -ILI9341 Gnd  = Gnd
 * -ILI9341 LED  = D0 (or 3.3v via 100R resistor)
 * 
 * Compiled with Arduino IDE v1.8.4
 * 
*/
#include <Wire.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;

// For the ESP8266 D1 Mini use these connections
#define TFT_DC   D3
#define TFT_CS   D8
#define TFT_MOSI D7
#define TFT_RST  D4
#define TFT_CLK  D5
#define TFT_LED  D0
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST); // Using only hardware SPI for speed

// Assign names to common 16-bit color values:
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define YELLOW   0xFFE0
#define WHITE    0xFFFF

const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte       rates[RATE_SIZE];      //Array of heart rates
byte       rateSpot = 0;
long       lastBeat = 0;          //Time at which the last beat occurred
float      beatsPerMinute;        // current BPM value
int        beatAvg      = 0,      // Current average BPM
           last_beatAvg = 0,
           gWidth       = 220,    // Graph width in pixels
           gHeight      = 160,    // Graph height in pixels
           gScale       = 2500,   // Graph scale 0-2500
           x_pos        = 85,     // Position of graph in x-axis
           y_pos        = 45,     // Position of graph in y-axis
           lastx1       = x_pos,  // Position of last drawing point
           lasty1       = y_pos,  // Position of last drawing point
           reading      = 1,      // reading counter
           maxima       = 0,      // Pulse IR maximum   
           minima       = 0;      // Pulse IRT minimum
String     system_title = "Blood Oxygen & BPM";

void setup()
{
  Serial.begin(115200);
  Serial.println("Initialising System...");
  tft.begin();                                                              // Start the TFT display
  tft.setRotation(3);                                                       // Rotate screen by 90°
  tft.setTextSize(2);                                                       // Set medium text size
  tft.setTextColor(WHITE);                                                  // Set text colour to white
  tft.fillScreen(BLACK);   // Clear the screen                              // Clear the screen 
  analogWriteFreq(500);    // Enable TFT display brightness                 // Pin D0 is used to adjust display brightness with PWM
  analogWrite(D0, 1000);    // Set display                                  // Activate the PWM on D0 lower values are dimmer e.g. 250
  DrawAxis(x_pos,y_pos,gWidth,gHeight,system_title);                        // Draw outline graph axis
  // Initialize sensor using I2C port and 400kHz speed
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))                          // Initialise the MAX30105 sensor
  {
    Serial.println("MAX30105 not found, please check connections/power");   // Report if not found
    while (1);
  }
  Serial.println("Place a finger on the sensor with steady pressure");      // Remind user to place finger on sensor
  byte ledBrightness = 0x1F; // Options: 0=Off to 255=50mA                  // Set MAX30105 parameters, see MAXIM data sheet for details
  byte sampleAverage = 4;    // Options: 1, 2, 4, 8, 16, 32
  byte ledMode       = 2;    // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate    = 100;  // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth     = 411;  // Options: 69, 118, 215, 411
  int adcRange       = 4096; // Options: 2048, 4096, 8192, 16384
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
}

void loop(){
  long irValue = particleSensor.getIR();                                    // Get current IR sensor reading
  if (checkForBeat(irValue) == true){                                       // Check for a valid beat, if so start reading the pulse rate
    long delta = millis() - lastBeat;                                       // Measure time since last check
    lastBeat   = millis();                                                  // Update last check time
    beatsPerMinute = 60 / (delta / 1000.0);                                 // Calculate beats-per-minute
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {                      // Check that beat-rate is in a normal range for most people
      rates[rateSpot++] = (byte)beatsPerMinute;                             // then store the reading in an array
      rateSpot %= RATE_SIZE;                                                // Wrap variable
      beatAvg = 0;                                                          // Take average of readings
      for (byte x = 0 ; x < RATE_SIZE ; x++) beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
  Serial.println("Avg BPM="+String(beatAvg));                               // Output beat rate on serial port
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  if (irValue > maxima ) maxima = irValue;                                  // Determine maximum value of IR reading
  if (irValue < minima ) minima = irValue;                                  // Determine minimum value of IR reading
  int difference = maxima - minima  + 50;                                   // Calcuate the difference and add a nominal offset
  int beat = irValue - difference + 2000;                                   // Adjust data for display
  Draw_Data(x_pos,y_pos,gWidth,gHeight,gScale,beat,YELLOW);                 // Display data
  tft.setTextColor(CYAN);
  if (irValue < 50000) {                                                    // Detect if no finger on sensor
    Serial.println(" No finger?");
    tft.setTextSize(2);
    tft.setTextColor(RED);
    String title = "No finger on sensor!";                                  // Say no finger on sensor
    tft.setCursor(x_pos + (gWidth - title.length()*11.5),y_pos+gHeight+9);  // Centre text across graph limits
    tft.print(title);
  } 
  else
  { 
    tft.fillRect(x_pos-15,y_pos+gHeight+9,250,25,BLACK);
    tft.setTextSize(4);
    tft.setCursor(10,100);
    if (reading%10==0){
      if (beatAvg != last_beatAvg) tft.fillRect(10,100,70,30,BLACK);        // Display BPM
      tft.print(beatAvg);
    }
    tft.setCursor(12,140);
    tft.setTextSize(2);
    tft.print("BPM");
  }
  reading = reading + 1;
  if (reading > gWidth) {                                                   // If reached end of graph, then rest back to the start
    reading = 1;
    lastx1  = x_pos;
    lasty1  = y_pos;
    maxima = 0; 
    minima = 0;
    DrawAxis(x_pos,y_pos,gWidth,gHeight,system_title);
  }
}

// Draw the graph axes
void DrawAxis(int x_pos, int y_pos, int width, int height, String title) {
  #define auto_scale_major_tick 5 // Sets the autoscale increment, so axis steps up in units of e.g. 5
  #define yticks 5                // 5 y-axis division markers
  tft.drawRect(x_pos,y_pos,width+2,height+3,WHITE);DrawLogo();//Draw boax outline
  tft.setTextSize(2);
  tft.setTextColor(GREEN);
  tft.setCursor(x_pos + (width - title.length()*12)/2,y_pos-20);// 12 pixels per char assumed at size 2 (10+2 pixels)
  tft.print(title);
  tft.fillRect(x_pos+1,y_pos+1,width-1,height,BLUE);
  tft.setTextSize(1);
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= yticks; spacing++) {
    #define number_of_dashes 40
    for (int j=0; j < number_of_dashes; j++){ // Draw dashed graph grid lines
      if (spacing < yticks) tft.drawFastHLine((x_pos+1+j*width/number_of_dashes),y_pos+(height*spacing/yticks),width/(2*number_of_dashes),WHITE);
    }
  }
}

// Draws the data point
void Draw_Data(int x_pos, int y_pos, int width, int height, int Y1Max, int BPM_Record, int graph_colour){
  #define auto_scale_major_tick 5 // Sets the autoscale increment, so axis steps up in units of e.g. 5
  #define yticks 5                // 5 y-axis division markers
  int x1,x2,y2;
  x1 = x_pos + reading * width/gWidth;
  x2 = x_pos + reading * width/gWidth; // max_readings is the global variable that sets the maximum data that can be plotted 
  y2 = y_pos + height - constrain(BPM_Record,0,Y1Max) * height / Y1Max + 1;
  tft.fillRect(x2,y_pos+1,2,height+1,BLUE);
  if (x1 < width+x_pos-2) tft.fillRect(x2+2,y_pos+1,2,height-0,RED);
  for (int spacing = 0; spacing <= yticks; spacing++) {
    #define number_of_dashes 40
    for (int j=0; j < number_of_dashes; j++){ // Draw dashed graph grid lines
      if (spacing < yticks) tft.drawFastHLine((x_pos+1+j*width/number_of_dashes),y_pos+(height*spacing/yticks),width/(2*number_of_dashes),WHITE);
    }
  }
  tft.drawLine(lastx1,lasty1,x2,y2,graph_colour);
  lastx1 = x2; lasty1 = y2;
}

void DrawLogo(){
  String title = "G6EJD";
  tft.drawRoundRect(5,5,title.length()*13,22,3,YELLOW);
  tft.setTextSize(2);
  tft.setCursor(8,9);
  tft.setTextColor(CYAN);
  tft.print("G6EJD");
}


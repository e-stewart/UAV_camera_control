//  -- LIBRARIES


// Logger:

#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

// GPS:
#include <TinyGPS.h>
#include <SoftwareSerial.h>


// --CONSTANTS


// pin designations
const int flashPin = 2; // pins 2 or 3 used for interupts!
const int txPin = 5; // GPS rx 
const int rxPin = 4; // GPS tx
const int focusPin = 6;
const int shutterPin = 7;
const int switchPin = 8; // start/stop cam trigger
const int statusLED = 9; 
// SD shield used 10!

byte masterSignalState = 0;


// Logger:

RTC_DS1307 RTC; // define the Real Time Clock object

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// the logging file
File logfile;

// error message:
void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);

  while(1);
}


// GPS

TinyGPS gps;
SoftwareSerial ss(rxPin, txPin);


// -- VARIABLES

unsigned long currentMillis = 0;    // stores the value of millis() in each iteration of loop()
unsigned long previousMillis = 0;
volatile boolean flashState = 0; //  flash state not used in main loop or setup. Declaring it as volotilw prevents compilers from exclucing it from program.
unsigned long time, date, speed, course;
long lat,lon;


// =====================================================================


void setup() {

 Serial.begin(9600);
 ss.begin(9600);
 
 //Serial.println("...");  // Debug - so we know what sketch is running

 // Set pin modes
 pinMode(flashPin, INPUT);
 pinMode(switchPin, INPUT_PULLUP); // Connect othrer side of switch to ground!
 pinMode(10, OUTPUT); // SD card
 pinMode(statusLED, OUTPUT);
 pinMode(focusPin, OUTPUT);
 pinMode(shutterPin, OUTPUT);

 digitalWrite(focusPin, LOW);
 digitalWrite(shutterPin, LOW);
 
 // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    error((char *)"Card failed, or not present");
  }
  //Serial.println("card initialized.");

  // Create log file:

  RTC.begin(); // Start real time clock
  delay(200);
  
  // If RTC is running, create logfile yearmonthday/hourminutesecond.CSV
  if (RTC.isrunning()) {
       
  char dirname[] = "00000000";
  char filename[] = "000000.CSV";

  char file[50]; //File name character storage
  char folder[50]; //Folder name character storage
  char location[1]; //location for file to be created
    
  // make filename from RTC
  getFilename(filename);
  Serial.println(filename);
  delay(200);
  
  // Make dir name from RTC:
  getDirname(dirname);
  Serial.println(dirname);
  delay(200);
  
  // If not present, create direcrory with todays date: 
  if (! SD.exists(dirname)){
    SD.mkdir(dirname);
  }
  delay(200);
  
  // concat dir & file name -> /dirname/filename
  strcpy(file,filename);
  strcpy(folder,dirname);
  Serial.println(folder);
  strcpy(location,"/");
  //strcat(location, folder);
  strcat(location, dirname);
  strcat(location, "/");
  strcat(location, file);
  //Serial.print("Location: ");
  //Serial.println(location);

  // create logfile:
  logfile = SD.open(location, FILE_WRITE); 
  
  }else{
    
    //Serial.println("logfile else loop");
    // If no RTC, create numbered log file:
    char filename[] = "LOGGER00.CSV";
    for (uint8_t i = 0; i < 100; i++) {
      filename[6] = i/10 + '0';
      filename[7] = i%10 + '0';
      if (! SD.exists(filename)) {
        //only open a new file if it doesn't exist
        logfile = SD.open(filename, FILE_WRITE); 
        break;  // leave the loop!
      }
    }
  }

  delay(200);
  if (! logfile) {
    error((char *)"couldnt create file");
  }
  
  // Write header to log file:
  
  
  logfile.println("lat,lon,date,time,speed,course");
  logfile.flush();
  
  // Start the I2C Bus as Slave on address 9
  Wire.begin(9);  

  // Attach a function to trigger when something is received.
  Wire.onReceive(masterSignal);

  // Set up interupt to detect flash:
  // Interupt main loop when flash is detected. Interupts can be detected on pins 2 & 3. Pin 2=interupt 0, pin 3=interupt 1.
  attachInterrupt(0, recordFlash, FALLING); // interupt=0(coz flash connected to pin 2) | function to run | when to trigger interupt


}// End setup


// =====================================================================


//  -- BEGIN LOOP

void loop() {

 getGPS();

 if (masterSignalState == 1){

   cameraTriggerSlave();
   masterSignalState=0;


 }

 
}

//-- FUNCTIONS

// Run when slave recieves signal from master
void masterSignal(int) {

 if (Wire.read()==1) {
  masterSignalState = 1;
 }
}
 
 
 void cameraTriggerSlave(){ 
  
  // Master signal is sent every 800 ms, the delay in sending & recieving the signal + delay in aquiring focus results in ~400 ms offset between cameras.
  digitalWrite(focusPin, HIGH);
  delay(300);

  digitalWrite(shutterPin, HIGH);
  delay(150);

  digitalWrite(shutterPin, LOW);
  digitalWrite(focusPin, LOW);
  
  delay(20); //debounce
 
  
}


long getGPS(){
    
  while(ss.available()){ // check for gps data
   
    if(gps.encode(ss.read())){ // encode gps data
      gps.get_position(&lat,&lon); // get latitude and longitude
      gps.get_datetime(&date, &time); // Get date, time and fix age
      speed=gps.speed(); // Get speed
      course=gps.course(); // Get course
    
    }
  }

return lat, lon, date, time, speed, course;

} // End get GPS function


void recordFlash() {

  flashState = digitalRead(flashPin); // Get pin state, should be high except when flash fires

  // Write GPS info to log file when camera flash is detected
  logfile.print(lat);
  logfile.print(",");
  logfile.print(lon);
  logfile.print(",");
  logfile.print(date);
  logfile.print(",");
  logfile.print(time);
  logfile.print(",");
  logfile.print(speed);
  logfile.print(",");
  logfile.println(course);
 
  logfile.flush();
 
  delay(10);

} // End record flash function


void getDirname(char *dirname) {
    
  // Get dir name from real time clock
  DateTime now = RTC.now(); int year = now.year(); int month = now.month(); int day = now.day();
  dirname[0] = '2';
  dirname[1] = '0';
  dirname[2] = (year/10)%10 + '0';
  dirname[3] = year%10 + '0';
  dirname[4] = month/10 + '0';
  dirname[5] = month%10 + '0';
  dirname[6] = day/10 + '0';
  dirname[7] = day%10 + '0';
    
  return;

} // End get dir name function


void getFilename(char *filename) {

  // Get logfile name from RTC:
  DateTime now = RTC.now(); int hour = now.hour(); int minute = now.minute(); int second = now.second();
  filename[0] = hour/10 + '0';
  filename[1] = hour%10 + '0';
  filename[2] = minute/10 + '0';
  filename[3] = minute%10 + '0';
  filename[4] = second/10 + '0';
  filename[5] = second%10 + '0';
  filename[6] = '.';
  filename[7] = 'C';
  filename[8] = 'S';
  filename[9] = 'V';
     
  return;
} // end get filename function
  
//-- END

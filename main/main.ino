//************|| Libraries used ||***********************
#include "VoiceRecognitionV3.h"
#include <WiFi.h>
#include "time.h"
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>
//---------------------------------------------

//**************|| Commands in the voice recognition module  ||************
#define iluminexa     (0)       //Enable the rest of the commands
#define red           (1)       //Screen in red
#define green         (2)       //Screen in green 
#define blue          (3)       //Screen in blue
#define temperature   (4)       //Print in the screen the temperature of the location given
#define time_         (5)       //Print in the screen the actual time (Spanish time) (Time is a reserved word, so we have to use time_)
#define party         (6)       //Random colors in each 
//---------------------------------------------------------

//***************|| Defines for the LED Matrix ||***********************
// Which pin on the Arduino is connected to the NeoPixels?
#define PIN_NEOPIXEL        33 // You can use what you want, not recommended 6 to 11 in ESP32.
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 256 //8x32 Matrix
//---------------------------------------------------------

//Object for the neopixel matrix
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Object for the voice recognition module
VR myVR(2);    // We use the UART hardware number 2 of the ESP32 (pin 16 is RX and 17 is TX, should be connected the Rx of the ESP32 to the Tx of the
               // voice recognition module and the Tx ESP32 to the Rx Voice Recognition)


//********|| Variables to connect to the wifi network ||***************************
const char* ssid       = "NameOfWifi";  //NAME AND PASSWORD OF YOUR WIFI
const char* password   = "PasswordOfWifi";
//---------------------------------------------------

//********|| Variables to get the time info ||***************************
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
struct tm timeinfo;
//---------------------------------------------------------

//********|| Variables to get the wheater info ||***************************
const String endpoint = "http://api.openweathermap.org/xxxxxxxxxxxxx";
const String key = "xxxxxxxxxxxxxxxx";
//------------------------------------------------------------------------

//********|| Variables for the voice recognition ||***************************
uint8_t records[7]; // You can only use 7 commands at the same time
uint8_t buf[64];
//---------------------------------------------------------

//********|| Variables for the main ||***************************
short enable=0, party_enable=0;
unsigned long now, time_command, time_party; //To control timing in the loop
//---------------------------------------------------------


//********|| Matrix for printing numbers in the LED Matrix ||***************************
//This contains the number of the pixels to print to form a number in the LED matrix.
//Due to the way is done, you can only print the numbers in a pair column (If the column is odd, the number will be upside down)
//Each number ocuppies 6 columns
short pix_matrix[10][30] = {
  {1,2,3,4,5,6,8,15,16,23,24,31,32,39,46,45,44,43,42,41},                         //zero
  {12,18,30,32,33,34,35,36,37,38,39},                                             //one
  {0,15,16,31,32,47,46,45,44,43,36,27,20,11,4,5,6,7,8,23,24,39,40},               //two
  {0,15,16,31,32,47,46,45,44,43,42,41,40,39,24,23,8,7,36,27,20,11,4},             //three
  {0,1,2,3,12,19,28,35,44,45,46,47,43,42,41,40},                                  //four
  {0,15,16,31,32,47,1,2,3,4,11,20,27,36,43,42,41,40,39,24,23,8,7},                //five
  {0,15,16,31,32,47,1,2,3,4,5,6,7,11,20,27,36,43,42,41,40,39,24,23,8,7},          //six
  {0,15,16,31,32,47,46,45,44,43,42,41,40},                                        //seven
  {0,15,16,31,32,47,1,2,3,4,5,6,7,11,20,27,36,43,42,41,40,39,24,23,8,7,46,45,44}, //eight
  {0,15,16,31,32,47,1,2,3,4,11,20,27,36,43,42,41,40,39,24,23,8,7,46,45,44}        //nine
};
short pix_long[10] = {20,11,23,23,16,23,26,13,29,26}; //You can use this in the print_time function for setting the pixels more efficiently
short pix_dots[4] =   {1,2,4,5};
//--------------------------------------------------------------------

//--------------------|| HERE START THE FUNCTIONS ||----------------------------------------

//**********|| FUNCTION THAT PRINTS THE GIVEN TIME IN THE LED MATRIX ||***************************
void print_time(int hours, int minutes)
{
  int j, pix, pix_max;
  int H1, H2, M1, M2;

  H1 = hours/10;
  H2 = hours%10;
  M1 = minutes/10;
  M2 = minutes%10;

  pixels.clear(); // Set all pixel colors to 'off';
//Set first digit of hours
  pix_max = pix_long[H1];
  for(j=0;j<pix_max;j++)
  {
    pix = pix_matrix[H1][j] + 8*0; //Print this number in the column 0
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));
  }
//Set second digit of hours
  pix_max = pix_long[H2];
  for(j=0;j<pix_max;j++)
  {
    pix = pix_matrix[H2][j] + 8*8;//Print this number in the column 8
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));    
  }
//Set the dots separation hours and minutes
  for(j=0;j<4;j++)
  {
    pix = pix_dots[j] + 8*15; //Print this dots in the column 15
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));
  }
//Set the first digit of minutes
  pix_max = pix_long[M1];
  for(j=0;j<pix_max;j++)
  {
    pix = pix_matrix[M1][j] + 8*18; //Print this number in the column 18
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));
  }
//Set the second digit of minutes
  pix_max = pix_long[M2];
  for(j=0;j<pix_max;j++)
  {
    pix = pix_matrix[M2][j] + 8*26; //Print this number in the column 26
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));    
  }

//Print the pixels set
   pixels.show();
}
//-----------------------------------------------------------------------


//*************|| FUNCTION TO PRINT THE LISTENING DOTS ||***************************
void print_listening()
{
  int j,pix, dot[12] = {2,3,4,5,10,11,12,13,18,19,20,21};
  pixels.clear(); // Set all pixel colors to 'off';
  //Set the first dot
  for(j=0;j<12;j++)
  {
    pix = dot[j] + 8*10; //Print this dot in the column 10
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));    
  }
  for(j=0;j<12;j++)
  {
    pix = dot[j] + 8*15; //Print this dot in the column 15
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));    
  }
  for(j=0;j<12;j++)
  {
    pix = dot[j] + 8*20; //Print this dot in the column 20
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));    
  }
  pixels.show();
  
}
//--------------------------------------------------------------------------


//******************|| FUNCTION TO GET THE WEATHER INFO ||***********************************
// If it returns -999 is a WiFi problem or if it returns -998 is a HTTP problem
int obtain_temp()
 {
    String payload, aux = "000"; //Its important to initialize the aux with 3 characters
    int temper;
    int index;
    if ((WiFi.status() != WL_CONNECTED)) { //Check the current connection status
      Serial.println("Error connecting to WiFi");
      return -999;
    }
    HTTPClient http; 
    http.begin(endpoint + key); //Specify the URL
    int httpCode = http.GET();  //Make the request
 
    if (httpCode > 0) 
    { //Check for the returning code
 
        payload=http.getString();//Obtain a string with a lot of info
        index=payload.indexOf("temp");//Search for the actual temperature
        aux[0]=payload[index+6]; //Save the temperature
        aux[1]=payload[index+7];
        aux[2]=payload[index+8];
        temper=aux.toInt(); //Transform it into integer
        temper-=273; //The temperature is given in Kelvin, so we convert it to Celsius
        http.end(); //Free the resources
        return temper;
     } 
     else 
     {
        Serial.println("Error on HTTP request");
        http.end(); //Free the resources
        return -998;
     }
 }
//-----------------------------------------------------------------------------------------------


//*****************|| FUNCTION TO PRINT THE TEMPERATURE IN THE LED MATRIX ||******************************
void print_temperature(int T)
{
  int j,pix_max,pix,minus[4] = {4,11,20,27};
  pixels.clear(); // Set all pixel colors to 'off';
  if(T < 0) //If it is a negative number, print the -
  {
    for(j = 0; j<4;j++);
    {
      pix = minus[j] + 8*6; //Print this sign in the column 8
      pixels.setPixelColor(pix, pixels.Color(50, 50, 50));
    }
  }
  T = abs(T);
  int T1 = T/10;
  int T2 = T%10;
  //For the first digit
  pix_max = pix_long[T1];
  for(j=0;j<pix_max;j++)
  {
    pix = pix_matrix[T1][j] + 8*12; //Print this digit in the column 14
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));    
  }
  //For the second digit
  pix_max = pix_long[T2];
  for(j=0;j<pix_max;j++)
  {
    pix = pix_matrix[T2][j] + 8*20; //Print this digit in the column 14
    pixels.setPixelColor(pix, pixels.Color(50, 50, 50));    
  }
  //Print the pixels that were set
  pixels.show();
}
//--------------------------------------------------------------------------------------


//********************|| PRINT THE PARTY IN THE LED MATRIX ||**********************
void print_party()
{
  int j;
  pixels.clear(); // Set all pixel colors to 'off';
  for(j=0;j<NUMPIXELS;j++)
  {
    pixels.setPixelColor( j, pixels.Color(random(0,50), random(0,50), random(0,50)) );  
  }
  pixels.show();
}
//----------------------------------------------------------------------------------


//*****************************|| SETUP ||**************************************
void setup()
{
  /** initialize */
  myVR.begin(9600); //Initialize the voice recognition
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  randomSeed(analogRead(26)); 
  
  Serial.begin(115200);
  if(myVR.clear() == 0){
    Serial.println("Recognizer cleared.");
  }else{
    Serial.println("Not find VoiceRecognitionModule.");
    Serial.println("Please check connection and restart ESP32.");
    while(1);
  }
  int cont_wifi = 0;
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      pixels.setPixelColor(cont_wifi, pixels.Color(50, 0, 0)); 
      pixels.show();
      cont_wifi++;
      
  }
  Serial.println(" CONNECTED");
  pixels.clear();
  pixels.show();
  
  //Init and get the time, it is saved, so we dont more wifi to get the 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Load the commands in the voice recognition module
  if(myVR.load((uint8_t)iluminexa) >= 0){
    Serial.println("iluminexa loaded");
  } 
  if(myVR.load((uint8_t)red) >= 0){
    Serial.println("rojo loaded");
  }
  if(myVR.load((uint8_t)green) >= 0){
    Serial.println("verde loaded");
  }
  if(myVR.load((uint8_t)blue) >= 0){
    Serial.println("azul loaded");
  }
  if(myVR.load((uint8_t)temperature) >= 0){
    Serial.println("temperatura loaded");
  }
  if(myVR.load((uint8_t)time_) >= 0){
    Serial.println("hora loaded");
  }
  if(myVR.load((uint8_t)party) >= 0){
    Serial.println("fiesta loaded");
  }
}
//------------------------------------------------------------------------------------

//****************************|| LOOP ||************************************************
void loop()
{
  now = millis();
  int ret;
  ret = myVR.recognize(buf, 50); //This recognize the voice and save the command in the buffer
  if(ret>0){
    switch(buf[1]){
      case iluminexa:
        Serial.println("Iluminexa");
        print_listening();
        enable = 1;
        time_command = now;
        break;
      case red:
        if(enable == 1)
        {
          Serial.println("Red");
          for(int i=0; i<(NUMPIXELS);i++)
          {
            pixels.setPixelColor(i, pixels.Color(100, 0, 0));
          }
        pixels.show();
        time_command = now;
        }
        break;
      case green:
        if(enable == 1)
        {
          Serial.println("green");
          for(int i=0; i<(NUMPIXELS);i++)
          {
            pixels.setPixelColor(i, pixels.Color(0, 100, 0));
          }
          pixels.show();
          time_command = now;
        }
        break;
      case blue:
        if(enable == 1)
        {
          Serial.println("blue");
          for(int i=0; i<(NUMPIXELS);i++)
          {
            pixels.setPixelColor(i, pixels.Color(0, 0, 1000));
          }
          pixels.show();
          time_command = now;
        }
        break;
      case temperature:
        if(enable == 1)
        {
          Serial.println("Temperature");
          int T = obtain_temp();
          Serial.println(T);
          if(T>-900) // //Error codes are -999(WiFi) and -998(HTTP)
          {
            print_temperature(T); //Print in the temperature in the LED Matrix.
            time_command = now;
          }
        }
        break;
      case time_:
        if(enable == 1)
        {
        Serial.println("Time");
        if(!getLocalTime(&timeinfo)){
          Serial.println("Failed to obtain time");
        }
        int hours = timeinfo.tm_hour;
        int minutes = timeinfo.tm_min;
        Serial.print(hours);
        Serial.print(":");
        Serial.println(minutes);
        print_time(hours,minutes);
        time_command = now;
        }
        break;
      case party:
        if (enable == 1)
        {
          Serial.println("Party");
          party_enable = 1;
          print_party();
          time_party = now;
          time_command = now;
        }
        break;                       
      default:
        Serial.println("Record function undefined");
        break;
    }
  }
  //This turn off the leds after 5 seconds from the last command
  if( (enable==1) && (now - time_command > 5000)   )
  {
    party_enable = 0;
    enable = 0;
    pixels.clear(); // Set all pixel colors to 'off';
    pixels.show(); //Turn off all pixels
  }
  //Change the colors of the party every 0.5 seconds
  if( (party_enable==1) && (now - time_party > 500) )
  {
    print_party();
    time_party = now;
  }
}
//-------------------------------------------------------------------------------------

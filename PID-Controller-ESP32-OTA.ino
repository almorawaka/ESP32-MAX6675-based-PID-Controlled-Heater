#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PIDController.h>
#include "max6675.h"
const char* host = "esp32";
const char* ssid = "Dialog 4G";
const char* password = "sema9454";

WebServer server(80);

const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

/*
 * setup function
 */
// Define Rotary Encoder Pins
#define CLK_PIN 4
#define DATA_PIN 2
#define SW_PIN 15
// MAX6675 Pins
#define thermoDO  14
#define thermoCS  12
#define thermoCLK  13
// Mosfet Pin
#define mosfet_pin 32
// Serial Enable







#define __DEBUG__
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
/*In this section we have defined the gain values for the 
 * proportional, integral, and derivative controller I have set
 * the gain values with the help of trial and error methods.
*/ 
#define __Kp 30 // Proportional constant
#define __Ki 0.7 // Integral Constant
#define __Kd 200 // Derivative Constant
const int buzzer = 27; //buzzer to arduino pin 36
const int ready = 26;
const int notReady = 25;
boolean startup_tone = true;
int clockPin=0; // Placeholder por pin status used by the rotary encoder
int clockPinState; // Placeholder por pin status used by the rotary encoder
float set_temperature = 30.0; // This set_temperature value will increas or decreas if when the rotarty encoder is turned
float temperature_value_c = 0.0; // stores temperature value
float temperature_cal_c = 0;  // lenear adjestment of temperature
unsigned long debounce = 0; // Debounce delay
int encoder_btn_count = 0; // used to check encoder button press
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO); // Create an instance for the MAX6675 Sensor Called "thermocouple"
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);// Create an instance for the SSD1306 128X64 OLED "display"
PIDController pid; // Create an instance of the PID controller class, called "pid"
void setup() {
#ifdef __DEBUG__
  Serial.begin(115200);
#endif
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
  pinMode (ready, OUTPUT);
  pinMode(notReady, OUTPUT);
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 27 as an output
  pinMode(mosfet_pin, OUTPUT); // MOSFET output PIN
  pinMode(CLK_PIN, INPUT); // Encoer Clock Pin
  pinMode(DATA_PIN, INPUT); //Encoder Data Pin
  pinMode(SW_PIN, INPUT_PULLUP);// Encoder SW Pin
  
  pid.begin();          // initialize the PID instance
  pid.setpoint(150);    // The "goal" the PID controller tries to "reach"
  pid.tune(__Kp, __Ki,__Kd);    // Tune the PID, arguments: kP, kI, kD
  pid.limit(0, 255);    // Limit the PID output between 0 and 255, this is important to get rid of integral windup!
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
#ifdef __DEBUG__
    Serial.println(F("SSD1306 allocation failed"));
#endif
    for (;;); // Don't proceed, loop forever
  }
  //
  display.setRotation(2); //Rotate the Display
  display.display(); //Show initial display buffer contents on the screen -- the library initializes this with an Adafruit splash screen.
  display.clearDisplay(); // Cleear the Display
  display.setTextSize(2); // Set text Size
  display.setTextColor(WHITE); // set LCD Colour
  display.setCursor(48, 0); // Set Cursor Position
  display.println("PID"); // Print the this Text
  display.setCursor(0, 20);  // Set Cursor Position
  display.println("Temperatur"); // Print the this Text
  display.setCursor(22, 40); // Set Cursor Position
  display.println("Control"); // Print the this Text
  display.display(); // Update the Display
  delay(2000); // Delay of 200 ms
}
void set_temp() {
  if (encoder_btn_count == 2) // check if the button is pressed twice and its in temperature set mode.
  {
    display.clearDisplay(); // clear the display
    display.setTextSize(2); // Set text Size
    display.setCursor(5, 0); // set the diplay cursor
    display.print("Set Temp."); // Print Set Temp. on the display
    display.setTextSize(4); // Set text Size
    display.setCursor(5, 30); // set the cursor
    display.print(set_temperature);// print the set temperature value on the display
    display.display(); // Update the Display
  }
}

void show_temp(){
    display.clearDisplay(); // Clear the display
    display.setTextSize(2); // Set text Size
    display.setCursor(5, 0); // Set the Display Cursor
    display.print("Cur Temp."); //Print to the Display
    display.setCursor(5, 30);// Set the Display Cursor
    display.setTextSize(4); // Set text Size
    display.print(temperature_value_c); // Print the Temperature value to the display in celcius
    display.display(); // Update the Display
    delay(200); // Wait 200ms to update the OLED dispaly.
  }


void read_encoder(){ // In this function we read the encoder data and increment the counter if its rotaing clockwise and decrement the counter if its rotating counter clockwis
while (encoder_btn_count==2){
      clockPin = digitalRead(CLK_PIN); // we read the clock pin of the rotary encoder
        if (clockPin != clockPinState  && clockPin == 1) { // if this condition is true then the encoder is rotaing counter clockwise and we decremetn the counter
            if (digitalRead(DATA_PIN) != clockPin) {
             set_temperature = set_temperature - 1;// decrmetn the counter.
              tone2();
              } else { 
                      set_temperature = set_temperature + .2;// Encoder is rotating CW so increment
                       tone2();
                        } 
              if (set_temperature < 20 )set_temperature = 20; // if the counter value is less than 1 the set it back to 1
              if (set_temperature > 38 ) set_temperature = 38; //if the counter value is grater than 150 then set it back to 150 
#ifdef __DEBUG__
    Serial.println(set_temperature); // print the set temperature value on the serial monitor window
#endif
  }
  clockPinState = clockPin; // Remember last CLK_PIN state
  if ( digitalRead(SW_PIN) == LOW)   //If we detect LOW signal, button is pressed
  {
    if ( millis() - debounce > 60) { //debounce delay
      encoder_btn_count++; // Increment the values 
      tone2();
      if (encoder_btn_count > 2) encoder_btn_count = 1;
#ifdef __DEBUG__
      Serial.println(encoder_btn_count);
#endif
    }
    debounce = millis(); // update the time variable
  }
   delay(1);// Put in a slight delay to help debounce the reading
   set_temp(); // Call the Set Temperature Function
}
}

int read_encorder_sw() {
  clockPinState = clockPin; // Remember last CLK_PIN state
  if ( digitalRead(SW_PIN) == LOW)   //If we detect LOW signal, button is pressed
  { if ( millis() - debounce > 60) { //debounce delay
       encoder_btn_count++; // Increment the values 
       tone2();
      if (encoder_btn_count > 2) encoder_btn_count = 1;
#ifdef __DEBUG__
      Serial.println(encoder_btn_count);
#endif
    }
    debounce = millis(); // update the time variable
  }
  return encoder_btn_count;
}

void tone1(){  // start up tone
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(1000);        // ...for 1 sec
  noTone(buzzer);     // Stop sound...
  delay(1000);        // ...for 1sec
}

void tone2(){ // tone to recogmize set values changes
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);        // ...for 1 sec
  noTone(buzzer);     // Stop sound...
  //delay(1000);        // ...for 1sec
}


void loop() // Put in a slight delay to help debounce the reading
{
  server.handleClient();
  delay(1);
  if(startup_tone){
      tone1();
      startup_tone=false;
    }
  encoder_btn_count = read_encorder_sw(); //read the encoeder switch value 1 or 2

  while(encoder_btn_count == 2){ 
   encoder_btn_count = read_encorder_sw(); //read the encoeder switch value 1 or 2
   analogWrite(mosfet_pin, 0); //  turn off heater untill set the temperature
   read_encoder(); //Call The Read Encoder Function to set the Temperature
     }
  while(encoder_btn_count == 1){ // check if the button is pressed and its in Free Running Mode -- in this mode the arduino continiously updates the screen and adjusts the PWM output according to the temperature.
    encoder_btn_count = read_encorder_sw(); //read the encoeder switch value 1 or 2
    temperature_value_c = thermocouple.readCelsius(); // Read the Temperature using the readCelsius methode from MAX6675 Library.
    temperature_value_c =  temperature_value_c +  temperature_cal_c; // linear adjstment of tempeture  
    int output = pid.compute(temperature_value_c);    // Let the PID compute the value, returns the optimal output
    analogWrite(mosfet_pin, output);           // Write the output to the output pin
    pid.setpoint(set_temperature); // Use the setpoint methode of the PID library to
    show_temp(); // show the current tem on display

    if(set_temperature-temperature_value_c>1 || temperature_value_c-set_temperature>1 ){
      digitalWrite(notReady,HIGH);
      digitalWrite(ready,LOW);
    }

    if(set_temperature-temperature_value_c<1 || temperature_value_c-set_temperature>1 ){
      digitalWrite(ready,HIGH);
      digitalWrite(notReady,LOW);
    }

    #ifdef __DEBUG__
    Serial.print(temperature_value_c); // Print the Temperature value in *C on serial monitor
    Serial.print("   "); // Print an Empty Space
    Serial.println(output); // Print the Calculate Output value in the serial monitor.
    #endif
  }
}

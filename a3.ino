///// Alex Banh, HCDE 440 sp19 assignment 3

#include <ESP8266WiFi.h>    //Requisite Libraries . . .
#include "Wire.h"           //
#include <PubSubClient.h>   //
#include <ArduinoJson.h>    //

//DHT required libraries
#include "DHT.h"
//MPL115A2 required libraries
#include <Wire.h>
#include <Adafruit_MPL115A2.h>
//OLED SSD1306 128x32 i2c required libraries
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET 13 // Reset pin # (or -1 if sharing Arduino reset pin)

#define wifi_ssid "Tell my Wi-fi love her"   //You have seen this before
#define wifi_password "thirstybanana810" //

#define DHTPIN 12     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Defines the digital pin that our button is connected to.
#define BUTTON_PIN 2

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Initialize MPL115A2 sensor
Adafruit_MPL115A2 mpl115a2;

// Initialize the OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//////////
// MQTT server address and credentials
//////////

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

//////////
//We also need to publish and subscribe to topics, for this sketch are going
//to adopt a topic/subtopic addressing scheme: topic/subtopic
//////////

WiFiClient espClient;             //blah blah blah, espClient
PubSubClient mqtt(espClient);     //blah blah blah, tie PubSub (mqtt) client to WiFi client

//////////
//We need a 'truly' unique client ID for our esp8266, all client names on the server must be unique.
//Every device, app, other MQTT server, etc that connects to an MQTT server must have a unique client ID.
//This is the only way the server can keep every device separate and deal with them as individual devices/apps.
//The client ID is unique to the device.
//////////

char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!

//////////
//In our loop(), we are going to create a c-string that will be our message to the MQTT server, we will
//be generous and give ourselves 200 characters in our array, if we need more, just change this number
//////////
unsigned long currentMillis, previousMillis; //we are using these to track the interval for our weather data packages

bool current = false; // Variable to store the status of the current from the button
String currentValue = "not pressed"; // String representing the current state of the button

/////SETUP/////
void setup() {
  Serial.begin(115200);
  // Setup our wifi and connect to the MQTT server
  setup_wifi();
  mqtt.setServer(mqtt_server, 1883);
  // Start up our sensors
  dht.begin();
  mpl115a2.begin();
  // set button pin as an input
  pinMode(BUTTON_PIN, INPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
}

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
}                                     //5C:CF:7F:F0:B0:C1 for example

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("theTopic/+"); //we are subscribing to 'theTopic' and all subtopics below that topic
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/////LOOP/////
void loop() {
  // Initialize our interval timer for sending weather data
  unsigned long currentMillis = millis();

  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'

  //Here we will deal with a JSON string


  // grab the current state of the button.
  // we have to flip the logic because we are
  // using a pullup resistor.
  if(digitalRead(BUTTON_PIN) == LOW) {
    current = true;
    currentValue = "pressed!";
  } else {
    current = false;
    currentValue = "not pressed!";
  }

  if (currentMillis - previousMillis > 5000) { //a periodic report, every 5 seconds

    // Reading temperature or humidity takes about 250 milliseconds!
      // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
      float h = dht.readHumidity();
      // Read temperature as Celsius (the default)
      float t = dht.readTemperature();
      // Read temperature as Fahrenheit (isFahrenheit = true)
      float f = dht.readTemperature(true);


  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  }

  // Variables initalized to store the current pressure and temperature
  float pressureKPA = 0, temperatureC = 0;

  // Gets and sets both the pressure and temperature variables with data from the MPL115A2 sensor
  mpl115a2.getPT(&pressureKPA,&temperatureC);


    /////
    //Unfortunately, as of this writing, sprintf (under Arduino) does not like floats  (bug!), so we can not
    //use floats in the sprintf function. Further, we need to send the temp and humidity as
    //a c-string (char array) because we want to format this message as JSON.
    //
    //We need to make these floats into c-strings via the function dtostrf(FLOAT,WIDTH,PRECISION,BUFFER).
    //To go from the float 3.14159 to a c-string "3.14" you would put in the FLOAT, the WIDTH or size of the
    //c-string (how many chars will it take up), the decimal PRECISION you want (how many decimal places, and
    //the name of a little BUFFER we can stick the new c-string into for a brief time. . .
    /////

    char str_temp[6]; //a temp array of size 6 to hold "XX.XX" + the terminating character
    char str_humd[6]; //a temp array of size 6 to hold "XX.XX" + the terminating character
    char str_pres[7]; //a temp array of size 7 to hold "XXX.XX" + the terminating character
    char curr_val[15]; //a temp array of size 15 to hold our button message

    //take temp, format it into 5 char array with a decimal precision of 2, and store it in str_temp
    dtostrf(f, 5, 2, str_temp);
    //ditto
    dtostrf(h, 5, 2, str_humd);
    //ditto2
    dtostrf(pressureKPA, 6, 2, str_pres);
    // Take our button message as a string and convert it into a character array
    currentValue.toCharArray(curr_val, 15);

    /////
    //For proper JSON, we need the "name":"value" pair to be in quotes, so we use internal quotes
    //in the string, which we tell the compiler to ignore by escaping the inner quotes with the '/' character
    /////

    sprintf(message, "{\"temp\":\"%s\", \"humd\":\"%s\", \"pres\":\"%s\", \"btn\":\"%s\"}", str_temp, str_humd, str_pres, curr_val);
    // Publish our weather data to the AlexBanh/a3 topic on our MQTT server
    mqtt.publish("AlexBanh/a3", message);

    // Calls a method to display the weather data on our LED screen
    displayWeather((String)f, (String)h, (String)pressureKPA);

     previousMillis = currentMillis;
 }


}//end Loop


 // displayWeather displays the current temperature, pressure, and humidity data on our screen.
void displayWeather(String temperature, String humidity, String pressure) {

  // Clears the current display
  display.clearDisplay();

  // Sets some display options
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Prints the current weather data to the OLED screen
  display.print("Current weather:\n");
  display.print("Temperature: " + temperature + " F\n");
  display.print("Humidity: " + humidity + " g/kg\n");
  display.print("Pressure: " + pressure + " kPa");


  // Tells the display to display what is buffered
  display.display();
}

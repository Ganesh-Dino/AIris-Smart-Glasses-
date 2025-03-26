//
// *** HINT: in case of an 'Sketch too Large' Compiler Warning/ERROR in Arduino IDE (ESP32 Dev Module):
// -> select a larger 'Partition Scheme' via menu > tools: e.g. using 'No OTA (2MB APP / 2MB SPIFFS) ***


/*
Library to be installed 

ESP32 Audio I2S -  https://github.com/schreibfaul1/ESP32-audioI2S
ArduinoJSON - https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
SimpleTimer - https://github.com/kiryanenko/SimpleTimer

*/


#define VERSION "\nitsdinothor AIris Solution Challenge"
#include <Wire.h>
#include <WiFi.h>  // only included here
#include <SD.h>    // also needed in other tabs (.ino)

#include <Audio.h>  // needed for PLAYING Audio (via I2S Amplifier, e.g. MAX98357) with ..
                    // Audio.h library from Schreibfaul1: https://github.com/schreibfaul1/ESP32-audioI2S
                    // .. ensure you have actual version (July 18, 2024 or newer needed for 8bit wav files!)
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



String text; 
String filteredAnswer = "";



#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels


#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32





const char* ssid = "WIFI NAME";                                                       // ## INSERT your wlan ssid
const char* password = "WIFI PASSWORD";                                                // ## INSERT your password

const char* gemini_KEY = "YOUR GEMINI API";                   //gemini api
String Max_Tokens="50";                                                          





#define AUDIO_FILE "/Audio.wav"  // mandatory, filename for the AUDIO recording




#define pin_RECORD_BTN 36

#define LED 2


// --- global Objects ----------

Audio audio_play;

// declaration of functions in other modules (not mandatory but ensures compiler checks correctly)
// splitting Sketch into multiple tabs see e.g. here: https://www.youtube.com/watch?v=HtYlQXt14zU

bool I2S_Record_Init();
bool Record_Start(String filename);
bool Record_Available(String filename, float* audiolength_sec);

String SpeechToText_Deepgram(String filename);

void displayText(String filteredAnswer);
void Deepgram_KeepAlive();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// ------------------------------------------------------------------------------------------------------------------------------
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Wire.begin();
  // Pin assignments:
  pinMode(LED, OUTPUT);
  pinMode(pin_RECORD_BTN, INPUT);  // use INPUT_PULLUP if no external Pull-Up connected ##
  // Hello World
  Serial.println(VERSION);
  // Connecting to WLAN
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WLAN ");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(". Done, device connected.");
  // Initialize SD card
  if (!SD.begin()) {
    Serial.println("ERROR - SD Card initialization failed!");
    return;
  }
  // initialize KALO I2S Recording Services (don't forget!)
  I2S_Record_Init();

 if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  } 
  // INIT done, starting user interaction
  Serial.println("> HOLD button for recording AUDIO .. RELEASE button for REPLAY & Deepgram transcription");
}



// ------------------------------------------------------------------------------------------------------------------------------
void loop() {
  if (digitalRead(pin_RECORD_BTN) == LOW)  // Recording started (ongoing)
  {
    digitalWrite(LED,HIGH);//  RED means 'Recording ongoing'
    delay(30);          // unbouncing & suppressing button 'click' noise in begin of audio recording
    //Start Recording
    Record_Start(AUDIO_FILE);
  }
  if (digitalRead(pin_RECORD_BTN) == HIGH)  // Recording not started yet .. OR stopped now (on release button)
  {
    digitalWrite(LED,LOW);

    float recorded_seconds;
    if (Record_Available(AUDIO_FILE, &recorded_seconds))  //  true once when recording finalized (.wav file available)
    {
      if (recorded_seconds > 0.4)  // ignore short btn TOUCH (e.g. <0.4 secs, used for 'audio_play.stopSong' only)
      {
        // ## Demo 1 - PLAY your own recorded AUDIO file (from SD card)
        // Hint to 8bit: you need AUDIO.H library from July 18,2024 or later (otherwise 8bit produce only loud (!) noise)
        // we commented out Demo 1 to jump to Demo 2 directly  .. uncomment once if you want to listen to your record !
        /*audio_play.connecttoFS(SD, AUDIO_FILE );              // play your own recorded audio  
        while (audio_play.isRunning()) {audio_play.loop();}     // wait here until done (just for Demo purposes)  */

        // ## Demo 2 [SpeechToText] - Transcript the Audio (waiting here until done)
        // led_RGB(HIGH, HIGH, LOW);  // BLUE means: 'Deepgram server creates transcription'

        String transcription = SpeechToText_Deepgram(AUDIO_FILE);

        //led_RGB(HIGH, LOW, HIGH);  // GREEN means: 'Ready for recording'
       
        digitalWrite(LED,LOW);
        Serial.println(transcription);

        //----------------------------------------------------
        WiFiClientSecure client;
        client.setInsecure();  // Disable SSL verification for simplicity (not recommended for production)
        String Answer = "";    // Declare Answer variable here

        text = "";

        if (client.connect("generativelanguage.googleapis.com", 443)) {
          String url = "/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(gemini_KEY);

          String payload = String("{\"contents\": [{\"parts\":[{\"text\":\"" + transcription + "\"}]}],\"generationConfig\": {\"maxOutputTokens\": " + Max_Tokens + "}}");


          // Send the HTTP POST request
          client.println("POST " + url + " HTTP/1.1");
          client.println("Host: generativelanguage.googleapis.com");
          client.println("Content-Type: application/json");
          client.print("Content-Length: ");
          client.println(payload.length());
          client.println();
          client.println(payload);

          // Read the response
          String response;
          while (client.connected()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") {
              break;
            }
          }

          // Read the actual response
          response = client.readString();
          parseResponse(response);
        } else {
          Serial.println("Connection failed!");
        }
        client.stop();  // End the connection
        //----------------------------------------------------
        if (filteredAnswer != "")  // we found spoken text .. now starting Demo examples:
        {      
          Serial.print("Gemini speaking: ");
          Serial.println(filteredAnswer);

          displayText(filteredAnswer);
          
         
        }
      }
    }
  }
// [Optional]: Stabilize WiFiClientSecure.h + Improve Speed of STT Deepgram response (~1 sec faster)
  // Idea: Connect once, then sending each 5 seconds dummy bytes (to overcome Deepgram auto-closing 10 secs after last request)
  // keep in mind: WiFiClientSecure.h still not 100% reliable (assuming RAM heap issue, rarely freezes after e.g. 10 mins)

  if (digitalRead(pin_RECORD_BTN) == HIGH && !audio_play.isRunning())  // but don't do it during recording or playing
  {
    static uint32_t millis_ping_before;
    if (millis() > (millis_ping_before + 5000)) {
      millis_ping_before = millis();
      digitalWrite(LED,HIGH); // short LED OFF means: 'Reconnection server, can't record in moment'
      Deepgram_KeepAlive();
    }
  }
}


// Revised section to handle response parsing
void parseResponse(String response) {
  
  // Extract JSON part from the response
  int jsonStartIndex = response.indexOf("{");
  int jsonEndIndex = response.lastIndexOf("}");

  if (jsonStartIndex != -1 && jsonEndIndex != -1) {
    String jsonPart = response.substring(jsonStartIndex, jsonEndIndex + 1);
    // Serial.println("Clean JSON:");
    // Serial.println(jsonPart);

    DynamicJsonDocument doc(1024);  // Increase size if needed
    DeserializationError error = deserializeJson(doc, jsonPart);

    if (error) {
      Serial.print("DeserializeJson failed: ");
      Serial.println(error.c_str());
      return;
    }

    if (doc.containsKey("candidates")) {
      for (const auto& candidate : doc["candidates"].as<JsonArray>()) {
        if (candidate.containsKey("content") && candidate["content"].containsKey("parts")) {

          for (const auto& part : candidate["content"]["parts"].as<JsonArray>()) {
            if (part.containsKey("text")) {
              text += part["text"].as<String>();
            }
          }
          text.trim();
          // Serial.print("Extracted Text: ");
          // Serial.println(text);
          filteredAnswer = "";
          for (size_t i = 0; i < text.length(); i++) {
            char c = text[i];
            if (isalnum(c) || isspace(c) || c == ',' || c == '.' || c == '\'') {
              filteredAnswer += c;
            } else {
              filteredAnswer += ' ';
            }
          }
          // filteredAnswer = text;
          // Serial.print("FILTERED - ");
          Serial.println(filteredAnswer);

          
        }
      }
    } else {
      Serial.println("No 'candidates' field found in JSON response.");
    }
  } else {
    Serial.println("No valid JSON found in the response.");
  }
}
// Function to display text on OLED
void displayText(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print(text.c_str());  // Convert String to char*
  display.display();
}



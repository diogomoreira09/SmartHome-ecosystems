// Matter Manager
#include <Matter.h>
#include <WiFi.h>
#include <Preferences.h>
#include "time.h"


// Matter Endpoints
MatterOnOffPlugin OnOffPlugin;   // Plug 1
MatterOnOffPlugin OnOffPlugin2;  // Plug 2

// Wi-Fi credentials
const char *ssid = "Vodafone-casa";
const char *password = "M31reles!";

// Preferences for storing state
Preferences matterPref;
const char *onOffPrefKey = "OnOff";     // For Plug 1
const char *onOff2PrefKey = "OnOff2";   // For Plug 2

// GPIO pin assignments
const uint8_t onoffPin = 5;             // Plug 1 relay
const uint8_t plug2Pin = 20;            // Plug 2 relay
const uint8_t plug2ButtonPin = 12;      // Button for Plug 2
const uint8_t buttonPin = BOOT_PIN;     // BOOT button for decommissioning

// Button state
uint32_t button_time_stamp = 0;
bool button_state = false;
const uint32_t decommissioningTimeout = 5000;

// Plug 2 button debounce
bool plug2ButtonPressed = false;
bool plug2State = false;
uint32_t plug2DebounceTime = 0;
const uint32_t debounceDelay = 200;

// Plug 1 callback
bool setPluginOnOff(bool state) {
  Serial.printf("User Callback :: New Plug 1 State = %s\r\n", state ? "ON" : "OFF");
  digitalWrite(onoffPin, state ? HIGH : LOW);
  matterPref.putBool(onOffPrefKey, state);
  return true;
}

// Plug 2 callback
bool setPlugin2OnOff(bool state) {
  Serial.printf("User Callback :: New Plug 2 State = %s\r\n", state ? "ON" : "OFF");
  digitalWrite(plug2Pin, state ? HIGH : LOW);
  plug2State = state;
  matterPref.putBool(onOff2PrefKey, state);
  return true;
}

void setup() {
  Serial.begin(115200);

  // Initialize GPIO
  pinMode(onoffPin, OUTPUT);
  pinMode(plug2Pin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(plug2ButtonPin, INPUT_PULLUP);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  // Initialize Preferences
  matterPref.begin("MatterPrefs", false);


  // Setup Plug 1 (pin 5)
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, false);
  OnOffPlugin.begin(lastOnOffState);
  OnOffPlugin.onChange(setPluginOnOff);
  digitalWrite(onoffPin, lastOnOffState ? HIGH : LOW);

  // Setup Plug 2 (pin 20)
  bool lastOnOffState2 = matterPref.getBool(onOff2PrefKey, false);
  OnOffPlugin2.begin(lastOnOffState2);
  OnOffPlugin2.onChange(setPlugin2OnOff);
  digitalWrite(plug2Pin, lastOnOffState2 ? HIGH : LOW);
  plug2State = lastOnOffState2;

  // Start Matter
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
    Serial.printf("Initial state: %s\r\n", OnOffPlugin.getOnOff() ? "ON" : "OFF");
    OnOffPlugin.updateAccessory();  // configure the Plugin based on initial state
  }
}

void loop() {
  // Commissioning check
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("\nMatter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());

    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {
        Serial.println("Waiting for commissioning...");
      }
    }
    OnOffPlugin.updateAccessory();
    OnOffPlugin2.updateAccessory();
    Serial.println("Matter Node is commissioned and connected to Wi-Fi.");
  }

  // BOOT button decommission logic
  if (digitalRead(buttonPin) == LOW && !button_state) {
    button_time_stamp = millis();
    button_state = true;
  }

  if (button_state && digitalRead(buttonPin) == HIGH) {
    button_state = false;
  }

  if (button_state && (millis() - button_time_stamp) > decommissioningTimeout) {
    Serial.println("Decommissioning the Matter device...");
    OnOffPlugin.setOnOff(false);
    OnOffPlugin2.setOnOff(false);
    Matter.decommission();
    button_time_stamp = millis();
  }

  // Plug 2 button handling
  bool currentButton = digitalRead(plug2ButtonPin) == LOW;
  if (currentButton && !plug2ButtonPressed && (millis() - plug2DebounceTime) > debounceDelay) {
    plug2ButtonPressed = true;
    plug2DebounceTime = millis();

    plug2State = !plug2State;
    digitalWrite(plug2Pin, plug2State ? HIGH : LOW);
    Serial.printf("Plug 2 toggled via button to %s\n", plug2State ? "ON" : "OFF");
    OnOffPlugin2.setOnOff(plug2State);
  }

  if (!currentButton) {
    plug2ButtonPressed = false;
  }
}

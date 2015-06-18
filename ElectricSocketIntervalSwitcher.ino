/*
  hydroval

  controls 4 relays for switching power outlets
  configurations can be set up via an lcd keypad shield
  it is possible to set up an interval and an power on time

  support for time based configuration will be added later
  when adding DS3231 clock module
*/

//INCLUDES//////////////////////////////////////////////////

#include <EEPROM.h>
#include "EEPROMAnything.h"
#include <LiquidCrystal.h>
#include <SimpleUI16x2.h>

//CONFIGURATION/////////////////////////////////////////////

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
SimpleUI16x2 simpleui(&lcd, getButton);

//The Button function you have to define by yourself
uint8_t getButton() {
  int adc_key_in = analogRead(0);
  if (adc_key_in > 790) return BUTTON_NONE;
  if (adc_key_in < 50) {
    return BUTTON_RIGHT;
  }
  if (adc_key_in < 195) {
    return BUTTON_UP;
  }
  if (adc_key_in < 380) {
    return BUTTON_DOWN;
  }
  if (adc_key_in < 555) {
    return BUTTON_LEFT;
  }
  if (adc_key_in < 790) {
    return BUTTON_SELECT;
  }
  return BUTTON_NONE;
}

//DEFINITIONS/////////////////////////////////////////////////

int relayPins[4] = {3, 2, 1, 0};

struct socketSettingStruct
{
  long intervalTime, onTime;
} socketSettings[4];

struct socketStateStruct
{
  long intervalCounter, onCounter;
} socketState[4];

unsigned long mainTimer = 0;

byte socketOff[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b11011,
  0b10001,
  0b01110,
  0b00000,
  0b00000
};

byte socketOn[8] = {
  0b00000,
  0b01110,
  0b11111,
  0b10101,
  0b11111,
  0b01110,
  0b00000,
  0b00000
};

//PROGRAM/////////////////////////////////////////////////////

void setup() {
  Serial.begin(9600);
  simpleui.write("hydrovasall", "by Kana");

  // create a new characters
  lcd.createChar(1, socketOff);
  lcd.createChar(2, socketOn);

  readSettingsFromEeprom();

  for (int i = 0; i < 4; i++) {
    // set all relays to off
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
    // set the temporary counters
    resetSocketCounter(i);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if (getButton() == BUTTON_SELECT) {
    uint16_t i;
    char* mainMenu[] = {"Run", "Configure", 0}; // last entry has to be 0
    simpleui.waitButtonRelease();
    switch (simpleui.showMenu("What do? D:", mainMenu)) {
      case 0:
        runIntervals();
        break;
      case 1:
        showConfigurationMenu();
        break;
    }
  } else if((millis() - mainTimer) >= 5000UL) {
    runIntervals();
  }
}

void readSettingsFromEeprom() {
  for (int i = 0; i < 4; i++) {
    EEPROM_readAnything(i * sizeof(socketSettingStruct), socketSettings[i]);
  }
}

void writeSettingsToEeprom(int socketNr, int intervalTime, int onTime) {
  socketSettings[socketNr].intervalTime = intervalTime;
  socketSettings[socketNr].onTime = onTime;
  EEPROM_writeAnything(socketNr * sizeof(socketSettingStruct), socketSettings[socketNr]);
}

void runIntervals() {
  while (getButton() != BUTTON_SELECT) {
    if ((millis() - mainTimer) >= 1000UL) {
    for (int i = 0; i < 4; i++) {
      if (socketState[i].intervalCounter > 0) {
        socketState[i].intervalCounter--;
      }

      if (socketState[i].intervalCounter <= 0) {
        socketState[i].onCounter--;
        if (socketState[i].onCounter < 0) {
          digitalWrite(relayPins[i], HIGH);
          socketState[i].intervalCounter = socketSettings[i].intervalTime;
          socketState[i].onCounter = socketSettings[i].onTime;
        } else {
          digitalWrite(relayPins[i], LOW);
        }
      }
    }
    showStatus();
    mainTimer = millis();
    }
  }
}

void showStatus() {
  // display socket symbols and seconds that indicate the status
  simpleui.clear();
  String statusStr;
  char statusText[6];
  
  for (int i = 0; i < 4; i++) {
      lcd.setCursor(0 + (i % 2) * 8, i / 2);
      if(socketState[i].intervalCounter <= 0) {
        lcd.write(2);
        
        if(socketState[i].onCounter >= 3600) {
          statusStr = String(socketState[i].onCounter/3600);
          statusStr.concat("h");
        } else if(socketState[i].onCounter >= 60) {
          statusStr = String(socketState[i].onCounter/60);
          statusStr.concat("m");
        } else {
          statusStr = String(socketState[i].onCounter);
          statusStr.concat("s");
        }
        statusStr.toCharArray(statusText, 6);
        
        simpleui.overwrite(i / 2, 2 + (i % 2) * 8, statusText);
      } else {
        lcd.write(1);
        
        if(socketState[i].intervalCounter >= 3600) {
          statusStr = String(socketState[i].intervalCounter/3600);
          statusStr.concat("h");
        } else if(socketState[i].intervalCounter >= 60) {
          statusStr = String(socketState[i].intervalCounter/60);
          statusStr.concat("m");
        } else {
          statusStr = String(socketState[i].intervalCounter);
          statusStr.concat("s");
        }
        statusStr.toCharArray(statusText, 6);
        
        simpleui.overwrite(i / 2, 2 + (i % 2) * 8, statusText);
      }
  }
}

void resetSocketCounter(int socketNr) {
  socketState[socketNr].intervalCounter = socketSettings[socketNr].intervalTime;
  socketState[socketNr].onCounter = socketSettings[socketNr].onTime;
}

void showConfigurationMenu() {
  char* menu[] = {"Socket 1", "Socket 2", "Socket 3", "Socket 4", 0}; //last entry has to be 0
  int iConfiguredSocket, iIntervalTime, iOnTime;
    
  switch (simpleui.showMenu("Configuration", menu)) {
    case 0:
      iIntervalTime = inputIntervalTime();
      delay(1000);
      iOnTime = inputOnTime();
      delay(1000);
      writeSettingsToEeprom(0, iIntervalTime, iOnTime);
      simpleui.write("Saved to EEPROM");
      resetSocketCounter(0);
      break;
    case 1:
      iIntervalTime = inputIntervalTime();
      delay(1000);
      iOnTime = inputOnTime();
      delay(1000);
      writeSettingsToEeprom(1, iIntervalTime, iOnTime);
      simpleui.write("Saved to EEPROM");
      resetSocketCounter(1);
      break;
    case 2:
      iIntervalTime = inputIntervalTime();
      delay(1000);
      iOnTime = inputOnTime();
      delay(1000);
      writeSettingsToEeprom(2, iIntervalTime, iOnTime);
      simpleui.write("Saved to EEPROM");
      resetSocketCounter(2);
      break;
    case 3:
      iIntervalTime = inputIntervalTime();
      delay(1000);
      iOnTime = inputOnTime();
      delay(1000);
      writeSettingsToEeprom(3, iIntervalTime, iOnTime);
      resetSocketCounter(3);
      simpleui.write("Saved to EEPROM");
      break;
  }
  
  delay(1000);
  runIntervals();
}

int inputIntervalTime() {
  char buffer[20];
  uint16_t i;

  simpleui.write("set interval:");
  i = simpleui.getUInt();
  simpleui.toString(i, buffer, 20);
  simpleui.write("interval set to");
  simpleui.overwrite(1, 0, buffer);
  simpleui.overwrite(1, strlen(buffer), "s");
  int iIntervalTime = strtol(buffer, NULL, 0);

  return iIntervalTime;
}

int inputOnTime() {  
  char buffer[20];
  uint16_t i;
  
  simpleui.write("set on time:");
  i = simpleui.getUInt();
  simpleui.toString(i, buffer, 20);
  simpleui.write("on time set to");
  simpleui.overwrite(1, 0, buffer);
  simpleui.overwrite(1, strlen(buffer), "s");
  int iOnTime = strtol(buffer, NULL, 0);

  return iOnTime;
}

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
#include "EEPROMTemplate.h"
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

int relayPins[4] = {2, 3, 11, 12};
int statusLedPins[4] = {A2, A3, A4, A5};
int manualOnPin = A1;

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
  // attention: serial begin takes over d0 and d1 pins!
  //Serial.begin(9600);

  // this is a workaround for SainSmart lcd shields which short pin10 to ground and slowly fry the AVR...
  pinMode(10, INPUT);

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
    
    // set status led pins to output
    pinMode(statusLedPins[i], OUTPUT);
    analogWrite(statusLedPins[i], 0);
  }

  pinMode(manualOnPin, INPUT_PULLUP);
}

void loop() {
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
    EEPROM_readStuff(i * sizeof(socketSettingStruct), socketSettings[i]);
  }
}

void writeSettingsToEeprom(int socketNr, int intervalTime, int onTime) {
  socketSettings[socketNr].intervalTime = intervalTime;
  socketSettings[socketNr].onTime = onTime;
  EEPROM_writeStuff(socketNr * sizeof(socketSettingStruct), socketSettings[socketNr]);
}

int readManualOnButtons() {
  int b = 255, c = 0;
  c = analogRead(manualOnPin); // get the analog value  
  if (c>1000) {
    b=255; // nothing pressed
  } else if (c>150 && c<200) {
    b=3; // button 4 pressed
  } else if (c>100 && c<150) {
    b=2; // button 3 pressed
  } else if (c>50 && c<100) {
    b=1; // button 2 pressed
  } else if (c<50) {
    b=0; // button 1 pressed
  }
  return b;
}

int relayState[4] = {0, 0, 0, 0};
int relayStateManualOverride[4] = {0, 0, 0, 0};
unsigned long lastOverride = 0;

void runIntervals() {
  while (getButton() != BUTTON_SELECT) {
    for (int i = 0; i < 4; i++) {
      if(((lastOverride == 0) || (millis() - lastOverride >= 250UL)) && (readManualOnButtons() == i)) {
        if((socketSettings[i].intervalTime > 0) || (socketSettings[i].onTime == 0)) {
          relayStateManualOverride[i] = !relayStateManualOverride[i];
          changeSocketState(i, relayStateManualOverride[i]);
        }
        lastOverride = millis();
      }
    }
    
    if ((millis() - mainTimer) >= 1000UL) {
    for (int i = 0; i < 4; i++) {
      if (socketState[i].intervalCounter > 0) {
        socketState[i].intervalCounter--;
      }
      if (socketState[i].intervalCounter <= 0) {
        socketState[i].onCounter--;
        if (socketState[i].onCounter < 0) {
          if(socketSettings[i].intervalTime > 0) {
            relayState[i] = 0;
          }
          socketState[i].intervalCounter = socketSettings[i].intervalTime;
          socketState[i].onCounter = socketSettings[i].onTime;
        } else {
          if((socketSettings[i].onTime > 0) || (socketSettings[i].intervalTime == 0)) {
            relayState[i] = 1;
          }
        }
      }
      if((relayState[i] == 0) && (relayStateManualOverride[i] == 0)) {
        changeSocketState(i, 0);
      }
      if((relayState[i] == 1) || (relayStateManualOverride[i] == 1)) {
        changeSocketState(i, 1);
      }      
    }

    showStatus();
    mainTimer = millis();
    }
  }
}

void changeSocketState(int iSocketNum, int iState) {
  if(iState == 1) {
    digitalWrite(relayPins[iSocketNum], LOW);
    analogWrite(statusLedPins[iSocketNum], 255);
  } else {
    digitalWrite(relayPins[iSocketNum], HIGH);
    analogWrite(statusLedPins[iSocketNum], 0);
  }
}

void showStatus() {
  // display socket symbols and seconds that indicate the status
  simpleui.clear();
  String statusStr;
  char statusText[6];
  
  for (int i = 0; i < 4; i++) {
      lcd.setCursor(0 + (i % 2) * 8, i / 2);
      
      if(socketSettings[i].onTime == 0) {
        lcd.write(1);
        //toReadableTime(socketState[i].onCounter, statusText);
        simpleui.overwrite(i / 2, 2 + (i % 2) * 8, "off");
        continue;
      }
      if(socketSettings[i].intervalTime == 0) {
        lcd.write(2);
        //toReadableTime(socketState[i].onCounter, statusText);
        simpleui.overwrite(i / 2, 2 + (i % 2) * 8, "on");
        continue;
      }
      
      if(socketState[i].intervalCounter <= 0) {
        lcd.write(2);
        toReadableTime(socketState[i].onCounter, statusText);
        simpleui.overwrite(i / 2, 2 + (i % 2) * 8, statusText);
      } else {
        lcd.write(1);        
        toReadableTime(socketState[i].intervalCounter, statusText);
        simpleui.overwrite(i / 2, 2 + (i % 2) * 8, statusText);
      }
  }
}

void toReadableTime(int seconds, char* status) {
  String statusStr;
  char statusText[6];
  
  if(seconds >= 3600) {
    statusStr = String(seconds/3600);
    statusStr.concat("h");
  } else if(seconds >= 60) {
    statusStr = String(seconds/60);
    statusStr.concat("m");
  } else {
    statusStr = String(seconds);
    statusStr.concat("s");
  }
  statusStr.toCharArray(status, 6);
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

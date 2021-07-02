/*
 * Created by Maxim Kliuba
 * e-mail: klyubamaks@gmail.com
 * (May - Juny 2021)
 */

#define DRIVER_STEP_TIME 10

#include <EncButton.h>
#include <GyverButton.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <GyverStepper.h>
#include <GyverTimers.h>
#include <TimeLib.h>
#include <GyverTimer.h>
#include <EEPROM.h>

struct Menu {
  String name;
  byte pageSize;
  byte itemsCount;
  int currentItem;
  bool firstPrint;
  bool update;
};

class MenuItem {
  public:
    String name;
  private:
    bool withParams;
    int minParamsValue;
    int maxParamsValue;
    byte step;

  public:
    MenuItem(String name_) {
      name = name_;
      withParams = false;
      minParamsValue = 0;
      maxParamsValue = 0;
      step = 0;
    }

    MenuItem(String name_, int minParamsValue_, int maxParamsValue_, byte step_) {
      name = name_;
      withParams = true;
      minParamsValue = minParamsValue_;
      maxParamsValue = maxParamsValue_;
      step = step_;
    }

    int incParamsValue(int value) {
      return constrain(value + step, minParamsValue, maxParamsValue);
    }

    int decParamsValue(int value) {
      return constrain(value - step, minParamsValue, maxParamsValue);
    }

    int getMinParamsValue() {
      return minParamsValue;
    }

    int getMaxParamsValue() {
      return maxParamsValue;
    }
    
    bool hasParams() {
      return withParams;
    }
};

struct Info {
  byte state;
  int currentMode;
};

enum DisplayPage {
  START_PAGE,
  MAIN_PAGE,
  MENU_PAGE,
  RECIPE_SETTINGS_PAGE,
  SETTINGS_PAGE,
  ABOUT_PAGE,
};

enum State {
  STOP,
  PAUSE,
  RUN,
};

enum ResipeState {
  NONE,
  STOP_UP,
  STOP_DOWN,
  STOP_LEFT,
  STOP_RIGHT,
  ROTATION,
};

const String initMessage[4] = {
  "Oops, this device is",
  "not activated",
  "Please contact me:",
  "klyubamaks@gmail.com",
};

const char *stateStr[] = {
  "STOP",
  "PAUSE",
  "RUN",
};

byte stopChar[8] = {
  0b00000,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b00000,
  0b00000,
};

byte pauseChar[8] = {
  0b00000,
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b00000,
  0b00000,
};

byte runChar[8] = {
  0b00000,
  0b01000,
  0b01100,
  0b01110,
  0b01100,
  0b01000,
  0b00000,
  0b00000,
};

byte pointerChar[8] = {
  0b00000,
  0b00100,
  0b00010,
  0b11111,
  0b00010,
  0b00100,
  0b00000,
  0b00000,
};

byte backChar[8] = {
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b00100,
  0b00100,
  0b11100,
  0b00000,
};

byte emptyBlockChar[8] = {
  0b11111,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b11111,
};

byte leftRotationBlockChar[8] = {
  0b11011,
  0b11101,
  0b10000,
  0b01101,
  0b01011,
  0b01111,
  0b10000,
  0b11111,
};

byte rightRotationBlockChar[8] = {
  0b11111,
  0b00001,
  0b11110,
  0b11010,
  0b10110,
  0b00001,
  0b10111,
  0b11011,
};

#define RESET_PERIOD 4000

#define ENCODER_S1_PIN 4
#define ENCODER_S2_PIN 3
#define ENCODER_KEY_PIN 2

#define STEPPER_0_STEP_PIN 6
#define STEPPER_0_DIR_PIN 5
#define STEPPER_0_EN_PIN 7

#define MAGNETIC_SENSOR_0_PIN 8
#define MAGNETIC_SENSOR_1_PIN 9

#define STEPPER_1_STEP_PIN 11
#define STEPPER_1_DIR_PIN 12
#define STEPPER_1_EN_PIN 10

#define LCD_ADDRESS 0x27  // 0x27 or 0x3f
#define LCD_COLS 20
#define LCD_ROWS 4

#define POINTER_CHAR 3
#define BACK_CHAR 4
#define LEFT_ROTATION_BLOCK_CHAR 5
#define RIGHT_ROTATION_BLOCK_CHAR 6
#define EMPTY_BLOCK_CHAR 7
#define BLOCK_CHAR 255
#define SPACE_CHAR 16

int getStepsPerRev(int spr, int s, int g, bool ms1, bool ms2, bool ms3);
void displayStartPage(bool forceClear = false);
void displayMainPage(bool forceClear = false);
void displayMenuPage(bool forceClear = false);
void displayRecipeSettingsPage(bool forceClear = false);
void displaySettingsPage(bool forceClear = false);
void displayAboutPage(bool forceClear = false);
void lcdPrint(const String text, byte x, byte y, byte maxLength = 0);
void printPointer(byte x, byte y, bool firstPrint = false, byte charCode = POINTER_CHAR);

MenuItem menuItems[] = {
  MenuItem("Recipe_1", 0, 1, 1),
  MenuItem("Recipe_2", 0, 1, 1),
  MenuItem("Recipe_3", 0, 1, 1),
  MenuItem("Recipe_4", 0, 1, 1),
  MenuItem("Recipe_5", 0, 1, 1),
  MenuItem("Recipe_6", 0, 1, 1),
  MenuItem("Settings"),
};

MenuItem recipeMenuItems[] = {
  MenuItem("Motor:", 0, 1, 1),
  MenuItem("Speed:", 1, 10, 1),
  MenuItem("Up", 0, 9, 1),
  MenuItem("Right", 0, 9, 1),
  MenuItem("Down", 0, 9, 1),
  MenuItem("Left", 0, 9, 1),
};

MenuItem settingsMenuItems[] = {
  MenuItem("SPR0:", 1, 9999, 1),
  MenuItem("S:", 1, 99, 1),
  MenuItem("G:", 1, 99, 1),
  MenuItem("1", 0, 1, 1),
  MenuItem("2", 0, 1, 1),
  MenuItem("3", 0, 1, 1),
  MenuItem("SPR1:", 1, 9999, 1),
  MenuItem("S:", 1, 99, 1),
  MenuItem("G:", 1, 99, 1),
  MenuItem("1", 0, 1, 1),
  MenuItem("2", 0, 1, 1),
  MenuItem("3", 0, 1, 1),
  MenuItem("Sensor0:", 0, 1, 1),
  MenuItem("Sensor1:", 0, 1, 1),
  MenuItem("Min speed:", 1, 720, 1),
  MenuItem("Max speed:", 1, 720, 1),
};

Menu mainMenu = {"<MENU>", LCD_ROWS - 1, (sizeof(menuItems) / sizeof(*menuItems)), 0, true, true};
Menu subMenu = {"<>", LCD_ROWS - 1, (sizeof(recipeMenuItems) / sizeof(*recipeMenuItems)), 0, true, true};
Menu settingsMenu = {"<SETTINGS>", LCD_ROWS - 1, (sizeof(settingsMenuItems) / sizeof(*settingsMenuItems)), 0, true, true};

// ------------------------ must be written to EEPROM ------------------------
const byte INIT_KEY = 15;

Info info = {STOP, -1};

int settingsParams[16] = {
  200, 10, 10, 1, 1, 1, 
  200, 10, 10, 1, 1, 1,
  1, 1, 
  36, 180,
};

byte recipeParams[6][6] = {
  {0, 1, 0, 0, 0, 0},
  {1, 1, 0, 0, 0, 0},
  {0, 1, 0, 0, 0, 0},
  {1, 1, 0, 0, 0, 0},
  {0, 1, 0, 0, 0, 0},
  {1, 1, 0, 0, 0, 0},
};

#define INIT_KEY_EEPROM_ADDRESS 15
#define INFO_EEPROM_ADDRESS (INIT_KEY_EEPROM_ADDRESS + sizeof(INIT_KEY))
#define SETTINGS_PARAMS_EEPROM_ADDRESS (INFO_EEPROM_ADDRESS + sizeof(info))
#define RECIPE_PARAMS_EEPROM_ADDRESS (SETTINGS_PARAMS_EEPROM_ADDRESS + sizeof(recipeParams))
// ----------------------------------------------------------------------------

EncButton<EB_TICK, ENCODER_S1_PIN, ENCODER_S2_PIN, ENCODER_KEY_PIN> enc;
GButton btn(ENCODER_KEY_PIN, HIGH_PULL, NORM_OPEN);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
GTimer timer(MS, 1000);
GStepper<STEPPER2WIRE> steppers[] = {
  GStepper<STEPPER2WIRE>(
      getStepsPerRev(
        settingsParams[0], 
        settingsParams[1], 
        settingsParams[2], 
        (bool)settingsParams[3], 
        (bool)settingsParams[4], 
        (bool)settingsParams[5]
      ), 
      STEPPER_0_STEP_PIN, STEPPER_0_DIR_PIN, STEPPER_0_EN_PIN
  ),
  GStepper<STEPPER2WIRE>(
      getStepsPerRev(
        settingsParams[6], 
        settingsParams[7], 
        settingsParams[8], 
        (bool)settingsParams[9], 
        (bool)settingsParams[10], 
        (bool)settingsParams[11]
      ), 
      STEPPER_1_STEP_PIN, STEPPER_1_DIR_PIN, STEPPER_1_EN_PIN),
};
GButton magneticSensors[] = {
  GButton(MAGNETIC_SENSOR_0_PIN, HIGH_PULL, NORM_OPEN),
  GButton(MAGNETIC_SENSOR_1_PIN, HIGH_PULL, NORM_OPEN),
};

DisplayPage lastDisplayedPage = START_PAGE;
DisplayPage currentPage = START_PAGE;
ResipeState resipeCurrentState = NONE;
ResipeState resipePrevState = NONE;
unsigned long time = 0;
unsigned long prevTime = 0;
bool updateMainPage = false;
bool isTimer2aEnadle = false;
int recipeStopParams[4] = { 
  0, 0, 0, 0, 
};

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.createChar((byte)STOP, stopChar);
  lcd.createChar((byte)PAUSE, pauseChar);
  lcd.createChar((byte)RUN, runChar);
  lcd.createChar(POINTER_CHAR, pointerChar);
  lcd.createChar(BACK_CHAR, backChar);
  lcd.createChar(LEFT_ROTATION_BLOCK_CHAR, leftRotationBlockChar);
  lcd.createChar(RIGHT_ROTATION_BLOCK_CHAR, rightRotationBlockChar);
  lcd.createChar(EMPTY_BLOCK_CHAR, emptyBlockChar);

  enableInterrupts();

  for(byte i = 0; i < (sizeof(steppers) / sizeof(*steppers)); i++) {
    steppers[i].autoPower(true);
  }

  displayStartPage(true);
}

void loop() {
  enc.tick();

  checkState();

  switch(currentPage) {
    case MAIN_PAGE: 
      displayMainPage(); 
      break;
    case MENU_PAGE: 
      displayMenuPage(); 
      break;
    case RECIPE_SETTINGS_PAGE: 
      displayRecipeSettingsPage(); 
      break;
    case SETTINGS_PAGE: 
      displaySettingsPage(); 
      break;
    case ABOUT_PAGE: 
      displayAboutPage(); 
      break;
  }
}

void checkState() {
  static State prevState = -1; 

  switch(info.state) {
    case RUN:
      if(info.currentMode != -1) {
        if(prevState != info.state) {
          resipePrevState = NONE;
          prevState = info.state;
          updateMainPage = true;
          goToStartPoint();
          timer.start();
        }
  
        if((recipeParams[info.currentMode][2] > recipeStopParams[0] / 60)) {
          resipeCurrentState = STOP_UP;
        }
        else if((recipeParams[info.currentMode][4] > recipeStopParams[2] / 60)) {
          resipeCurrentState = STOP_DOWN;
        }
        else if((recipeParams[info.currentMode][5] > recipeStopParams[3] / 60)) {
          resipeCurrentState = STOP_LEFT;
        }
        else if((recipeParams[info.currentMode][3] > recipeStopParams[1] / 60)) {
          resipeCurrentState = STOP_RIGHT;
        }
        else {
          resipeCurrentState = ROTATION;
          if(steppers[recipeParams[info.currentMode][0]].getSpeedDeg() != getParamsSpeedDeg()) {
            setStepperSpeedDeg(recipeParams[info.currentMode][0], getParamsSpeedDeg());
          }
        }

        if (timer.isReady()) {
          time++;

          switch(resipeCurrentState) {
            case STOP_UP:
              recipeStopParams[0]++;
              break;
            case STOP_DOWN:
              recipeStopParams[2]++;
              break;
            case STOP_LEFT:
              recipeStopParams[3]++;
              break;
            case STOP_RIGHT:
              recipeStopParams[1]++; 
              break;
          }
        }
  
        if(resipePrevState != resipeCurrentState) {
          switch(resipeCurrentState) {
            case STOP_UP:
              steppers[recipeParams[info.currentMode][0]].setRunMode(FOLLOW_POS);
              setStepperSpeedDeg(recipeParams[info.currentMode][0], settingsParams[15]);
              steppers[recipeParams[info.currentMode][0]].setTargetDeg(180);
              break;
            case STOP_DOWN:
              steppers[recipeParams[info.currentMode][0]].setRunMode(FOLLOW_POS);
              setStepperSpeedDeg(recipeParams[info.currentMode][0], settingsParams[15]);
              steppers[recipeParams[info.currentMode][0]].setTargetDeg(0);
              break;
            case STOP_LEFT:
              steppers[recipeParams[info.currentMode][0]].setRunMode(FOLLOW_POS);
              setStepperSpeedDeg(recipeParams[info.currentMode][0], settingsParams[15]);
              steppers[recipeParams[info.currentMode][0]].setTargetDeg(90);
              break;
            case STOP_RIGHT:
              steppers[recipeParams[info.currentMode][0]].setRunMode(FOLLOW_POS);
              setStepperSpeedDeg(recipeParams[info.currentMode][0], settingsParams[15]);
              steppers[recipeParams[info.currentMode][0]].setTargetDeg(270);
              break;
            case ROTATION:
              steppers[recipeParams[info.currentMode][0]].setRunMode(KEEP_SPEED);
              setStepperSpeedDeg(recipeParams[info.currentMode][0], getParamsSpeedDeg());
              break;
          }
          resipePrevState = resipeCurrentState;
          updateMainPage = true;
        }  
      }
      break;
    case PAUSE:
      if(prevState != info.state) {
        resipePrevState = NONE;
        prevState = info.state;
        updateMainPage = true;
        goToStartPoint();
      }
      break;
    case STOP:
      if(prevState != info.state) {
        resipePrevState = NONE;
        prevState = info.state;        
        updateMainPage = true;
        goToStartPoint();
        time = 0;
        for(byte i = 0; i < 4; i++) {
          recipeStopParams[i] = 0;
        }
        info.currentMode = -1;
      }
      break;
  }
}

void displayStartPage(bool forceClear = false) {
  if (lastDisplayedPage != currentPage || forceClear) {
    lastDisplayedPage = currentPage;
    lcd.clear();
  }

  bool resetFlag = false;

  btn.tick();  
  if (btn.state()) {
    unsigned long resetTimer = millis();
    unsigned long counterTimer = millis();

    lcd.setCursor(4, 1);
    lcd.print("RESETTING");

    while (btn.state()) {
      btn.tick();
      
      if(millis() - counterTimer >= 1000) {
        lcd.print(".");
        counterTimer = millis();
      }

      if(millis() - resetTimer >= RESET_PERIOD) {      
        resetFlag = true;
        break;
      }
    }
    
    lcd.clear();        
    if(!resetFlag) {
      lcdPrintCenter("FAIL", 1);
      delay(1000);
    }
  }

  disableInterrupts();
  if(EEPROM.read(INIT_KEY_EEPROM_ADDRESS) != INIT_KEY) {
    lcd.clear();
    lcdPrintCenter(initMessage[0], 0);
    lcdPrintCenter(initMessage[1], 1);
    lcdPrint(initMessage[2], 0, 2);
    lcdPrint(initMessage[3], 0, 3);
    while(1);
  }
  
  if(EEPROM.read(INIT_KEY_EEPROM_ADDRESS) != INIT_KEY || resetFlag) {
    EEPROM.update(INIT_KEY_EEPROM_ADDRESS, INIT_KEY);
    EEPROM.put(INFO_EEPROM_ADDRESS, info);
    EEPROM.put(SETTINGS_PARAMS_EEPROM_ADDRESS, settingsParams);
    EEPROM.put(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
  
    lcdPrintCenter("SUCCESS", 1);
    delay(1000);
    resetFlag = false;
  }

  EEPROM.get(INFO_EEPROM_ADDRESS, info);
  EEPROM.get(SETTINGS_PARAMS_EEPROM_ADDRESS, settingsParams);
  EEPROM.get(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
  enableInterrupts();

  lcd.clear();
  lcdPrintCenter("By Igor Serdiuk", 1);
  delay(1000);
  currentPage = MAIN_PAGE;
}

void displayMainPage(bool forceClear = false) {
  if (lastDisplayedPage != currentPage || forceClear) {
    lastDisplayedPage = currentPage;
    lcd.clear();
    updateMainPage = true;
  }

  if(enc.isLeft()) {
    if(info.state != RUN) {
      for(byte i = 0; i < (sizeof(steppers) / sizeof(*steppers)); i++) {
        steppers[i].setRunMode(FOLLOW_POS);
        setStepperSpeedDeg(i, settingsParams[15]);
        steppers[i].setTargetDeg(-9, RELATIVE);
      }
    }
  }
  else if(enc.isRight()) {
    if(info.state != RUN) {
      for(byte i = 0; i < (sizeof(steppers) / sizeof(*steppers)); i++) {
        steppers[i].setRunMode(FOLLOW_POS);
        setStepperSpeedDeg(i, settingsParams[15]);
        steppers[i].setTargetDeg(9, RELATIVE);
      }
    }
  }

  if (enc.isLeftH()) {
    if(info.currentMode != -1) {
      recipeParams[info.currentMode][1] = recipeMenuItems[1].decParamsValue(recipeParams[info.currentMode][1]);
      updateMainPage = true;

      disableInterrupts();
      EEPROM.put(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
      enableInterrupts();
    }
  }
  else if (enc.isRightH()) {
    if(info.currentMode != -1) {
      recipeParams[info.currentMode][1] = recipeMenuItems[1].incParamsValue(recipeParams[info.currentMode][1]);
      updateMainPage = true;

      disableInterrupts();
      EEPROM.put(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
      enableInterrupts();
    }
  }

  if (enc.isHolded()) {
    if(info.state == RUN) {
      info.state = PAUSE;
      updateMainPage = true;
    }
    currentPage = MENU_PAGE;

    disableInterrupts();
    EEPROM.put(INFO_EEPROM_ADDRESS, info);
    enableInterrupts();
  }

  if (enc.hasClicks()) {
    switch(enc.clicks) {
      case 1:
        if(info.currentMode != -1 && info.state == RUN) {
          info.state = PAUSE;
        }
        else if(info.currentMode != -1 && info.state == PAUSE) {
          info.state = RUN;
        }
        updateMainPage = true;
        break;
      case 2:
        if(info.currentMode != -1) {
          info.state = STOP;
          updateMainPage = true;
        }
        break;
      case 3:
        if(info.state != RUN) {
          goToStartPoint();
        }
        break;
      case 4:
        if(info.state != RUN) {
          resetMotors();
        }
        break;
    }

    disableInterrupts();
    EEPROM.put(INFO_EEPROM_ADDRESS, info);
    enableInterrupts();
  }

  if(prevTime != time) {
    prevTime = time;
    if(info.currentMode != -1) {
      lcdPrint(formatTime(time), 0, 0, 8);
    }
    else {
      lcdPrint("--:--:--", 0, 0, 8);
    }
  }

  if(updateMainPage) {
    lcdPrintChar(info.state, 13, 0);
    lcdPrint(stateStr[info.state], 15, 0, 5);
    if(info.currentMode != -1) {
      lcdPrint(formatTime(time), 0, 0, 8);
      lcdPrint("<" + menuItems[info.currentMode].name + ">", 0, 1);
      lcdPrint("Motor:" + (String)recipeParams[info.currentMode][0], 1, 2, 8);
      lcdPrint("Speed:" + (String)recipeParams[info.currentMode][1], 1, 3, 8);
      for(byte i = 0; i < 4; i++) {
        lcdPrintChar(BLOCK_CHAR, 13 + i, 2);
      }
      switch(resipeCurrentState) {
        case STOP_UP:
          lcdPrintChar(POINTER_CHAR, 13, 1);
          lcdPrintChar(SPACE_CHAR, 17, 2);
          lcdPrintChar(SPACE_CHAR, 13, 3);
          lcdPrintChar(SPACE_CHAR, 9, 2);
          break;
        case STOP_DOWN:
          lcdPrintChar(SPACE_CHAR, 13, 1);
          lcdPrintChar(SPACE_CHAR, 17, 2);
          lcdPrintChar(POINTER_CHAR, 13, 3);
          lcdPrintChar(SPACE_CHAR, 9, 2);
          break;
        case STOP_LEFT:
          lcdPrintChar(SPACE_CHAR, 13, 1);
          lcdPrintChar(SPACE_CHAR, 17, 2);
          lcdPrintChar(SPACE_CHAR, 13, 3);
          lcdPrintChar(POINTER_CHAR, 9, 2);
          break;
        case STOP_RIGHT:
          lcdPrintChar(SPACE_CHAR, 13, 1);
          lcdPrintChar(POINTER_CHAR, 17, 2);
          lcdPrintChar(SPACE_CHAR, 13, 3);
          lcdPrintChar(SPACE_CHAR, 9, 2);
          break;
        case ROTATION:
          lcdPrintChar(LEFT_ROTATION_BLOCK_CHAR, 14, 2);
          lcdPrintChar(RIGHT_ROTATION_BLOCK_CHAR, 15, 2);
          lcdPrintChar(SPACE_CHAR, 13, 1);
          lcdPrintChar(SPACE_CHAR, 17, 2);
          lcdPrintChar(SPACE_CHAR, 13, 3);
          lcdPrintChar(SPACE_CHAR, 9, 2);
          break;
      }
      lcdPrint((String)recipeParams[info.currentMode][2] + "m", 14, 1);
      lcdPrint((String)recipeParams[info.currentMode][3] + "m", 18, 2);
      lcdPrint((String)recipeParams[info.currentMode][4] + "m", 14, 3);
      lcdPrint((String)recipeParams[info.currentMode][5] + "m", 10, 2);
      
    }
    else {
      lcd.clear();
      lcdPrint("--:--:--", 0, 0, 8);
      lcdPrintChar(info.state, 13, 0);
      lcdPrint(stateStr[info.state], 15, 0, 5);
      lcdPrintCenter("<NOT SELECTED>", 2);
    }
    updateMainPage = false;
  }
}

void displayMenuPage(bool forceClear = false) {
  if (lastDisplayedPage != currentPage || forceClear) {
    lastDisplayedPage = currentPage;
    lcd.clear();
    lcdPrintCenter(mainMenu.name, 0);
    mainMenu.currentItem = 0;
    mainMenu.firstPrint = true;
    mainMenu.update = true;
  }
  else if(mainMenu.firstPrint){
    mainMenu.firstPrint = false;
  }

  if (enc.isLeft()) {
    mainMenu.currentItem = constrain(mainMenu.currentItem - 1, -1, mainMenu.itemsCount - 1);
    mainMenu.update = true;
  }
  else if (enc.isRight()) {
    mainMenu.currentItem = constrain(mainMenu.currentItem + 1, -1, mainMenu.itemsCount - 1);
    mainMenu.update = true;
  }
  else if (enc.isLeftH()) {
    if(mainMenu.currentItem != -1 && menuItems[mainMenu.currentItem].hasParams()) {
      recipeParams[mainMenu.currentItem][0] = menuItems[mainMenu.currentItem].decParamsValue(recipeParams[mainMenu.currentItem][0]);
      if(mainMenu.currentItem == info.currentMode) {
        time = 0;
        for(byte i = 0; i < 4; i++) {
          recipeStopParams[i] = 0;
        }
      }
      mainMenu.update = true;
    }
  }
  else if (enc.isRightH()) {
    if(mainMenu.currentItem != -1 && menuItems[mainMenu.currentItem].hasParams()) {
      recipeParams[mainMenu.currentItem][0] = menuItems[mainMenu.currentItem].incParamsValue(recipeParams[mainMenu.currentItem][0]);
      if(mainMenu.currentItem == info.currentMode) {
        time = 0;
        for(byte i = 0; i < 4; i++) {
          recipeStopParams[i] = 0;
        }
      }
      mainMenu.update = true;
    }
  }

  if (enc.hasClicks()) {
    switch(enc.clicks) {
      case 1: 
        if(mainMenu.currentItem == -1) {
          currentPage = MAIN_PAGE;
        }
        else if(mainMenu.currentItem == mainMenu.itemsCount - 1) {
          currentPage = SETTINGS_PAGE;
        }
        else {
          currentPage = RECIPE_SETTINGS_PAGE;
        }
        break;
      case 5: 
        currentPage = ABOUT_PAGE;
        break;
    }

    disableInterrupts();
    EEPROM.put(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
    EEPROM.put(INFO_EEPROM_ADDRESS, info);
    enableInterrupts();
  }

  if (enc.isHolded()) {
    currentPage = MAIN_PAGE;
        
    disableInterrupts();
    EEPROM.put(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
    EEPROM.put(INFO_EEPROM_ADDRESS, info);
    enableInterrupts();
  }

  if(mainMenu.update && mainMenu.currentItem != -1) {
    for(byte i = 0; i < mainMenu.pageSize; i++) {
      byte itemIndex = (mainMenu.currentItem > mainMenu.pageSize - 1 ? i + (mainMenu.currentItem - (mainMenu.pageSize - 1)) : i);
      if(menuItems[itemIndex].hasParams()) {
        lcdPrint(menuItems[itemIndex].name + " [M:" + recipeParams[itemIndex][0] + "]", 1, i + 1, 14);
      }
      else {
        lcdPrint(menuItems[itemIndex].name, 1, i + 1, 14);
      }
    }
    mainMenu.update = false;
    printPointer(0, constrain(mainMenu.currentItem, 0, mainMenu.pageSize - 1) + 1, mainMenu.firstPrint);
  }
  else if(mainMenu.currentItem == -1) {
    printPointer(19, 0, mainMenu.firstPrint, BACK_CHAR);
  }
}

void displayRecipeSettingsPage(bool forceClear = false) {
  if (lastDisplayedPage != currentPage || forceClear) {
    lastDisplayedPage = currentPage;
    lcd.clear();
    subMenu.name = "<" + menuItems[mainMenu.currentItem].name + ">";
    lcdPrintCenter(subMenu.name, 0);
    for(byte i = 0; i < 4; i++) {
      lcdPrintChar(BLOCK_CHAR, 13 + i, 2);
    }
    subMenu.firstPrint = true;
    subMenu.update = true;
    subMenu.currentItem = 0;
  }
  else if(subMenu.firstPrint){
    subMenu.firstPrint = false;
  }

  if (enc.isLeft()) {
    subMenu.currentItem = constrain(subMenu.currentItem - 1, -1, subMenu.itemsCount - 1);
    subMenu.update = true;
  }
  else if (enc.isRight()) {
    subMenu.currentItem = constrain(subMenu.currentItem + 1, -1, subMenu.itemsCount - 1);
    subMenu.update = true;
  }
  else if (enc.isLeftH()) {
    if(subMenu.currentItem != -1 && recipeMenuItems[subMenu.currentItem].hasParams()) {
      recipeParams[mainMenu.currentItem][subMenu.currentItem] = 
          recipeMenuItems[subMenu.currentItem].decParamsValue(recipeParams[mainMenu.currentItem][subMenu.currentItem]);          
      if(mainMenu.currentItem == info.currentMode && subMenu.currentItem == 0) {
        time = 0;
        for(byte i = 0; i < 4; i++) {
          recipeStopParams[i] = 0;
        }
      }
      subMenu.update = true;
    }
  }
  else if (enc.isRightH()) {
    if(subMenu.currentItem != -1 && recipeMenuItems[subMenu.currentItem].hasParams()) {
      recipeParams[mainMenu.currentItem][subMenu.currentItem] = 
          recipeMenuItems[subMenu.currentItem].incParamsValue(recipeParams[mainMenu.currentItem][subMenu.currentItem]);        
      if(mainMenu.currentItem == info.currentMode && subMenu.currentItem == 0) {
        time = 0;
        for(byte i = 0; i < 4; i++) {
          recipeStopParams[i] = 0;
        }
      }   
      subMenu.update = true;
    }
  }

  if (enc.hasClicks()) {
    switch(enc.clicks) {
      case 1:
        if(subMenu.currentItem != -1) {
          startCurrentMode(mainMenu.currentItem);
          currentPage = MAIN_PAGE;
        }
        else {
          currentPage = MENU_PAGE;
        }
        break;
    }

    disableInterrupts();
    EEPROM.put(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
    EEPROM.put(INFO_EEPROM_ADDRESS, info);
    enableInterrupts();
  }

  if (enc.isHolded()) {
    currentPage = MENU_PAGE;
    
    disableInterrupts();
    EEPROM.put(RECIPE_PARAMS_EEPROM_ADDRESS, recipeParams);
    EEPROM.put(INFO_EEPROM_ADDRESS, info);
    enableInterrupts();
  }

  if(subMenu.update) {
    lcdPrint(recipeMenuItems[0].name + (String)recipeParams[mainMenu.currentItem][0], 1, 1, 8);
    lcdPrint(recipeMenuItems[1].name + (String)recipeParams[mainMenu.currentItem][1], 1, 2, 8);
    lcdPrint((String)recipeParams[mainMenu.currentItem][2] + "m", 14, 1);
    lcdPrint((String)recipeParams[mainMenu.currentItem][3] + "m", 18, 2);
    lcdPrint((String)recipeParams[mainMenu.currentItem][4] + "m", 14, 3);
    lcdPrint((String)recipeParams[mainMenu.currentItem][5] + "m", 10, 2);
    subMenu.update = false;
  }

  switch(subMenu.currentItem) {
    case -1:
      printPointer(19, 0, subMenu.firstPrint, BACK_CHAR);
      break;
    case 0:
      printPointer(0, 1, subMenu.firstPrint);
      break;
    case 1:
      printPointer(0, 2, subMenu.firstPrint);
      break;
    case 2:
      printPointer(13, 1, subMenu.firstPrint);
      break;
    case 3:
      printPointer(17, 2, subMenu.firstPrint);
      break;
    case 4:
      printPointer(13, 3, subMenu.firstPrint);
      break;
    case 5:
      printPointer(9, 2, subMenu.firstPrint);
      break;
  }
}

void displaySettingsPage(bool forceClear = false) {
  static byte prevMenuPage = 0;
  static byte currentMenuPage = 0;
  
  if (lastDisplayedPage != currentPage || forceClear) {
    lastDisplayedPage = currentPage;
    lcd.clear();
    lcdPrintCenter(settingsMenu.name, 0);
    settingsMenu.firstPrint = true;
    settingsMenu.update = true;
    settingsMenu.currentItem = 0;
    prevMenuPage = 0;
    currentMenuPage = 0;
  }
  else if(settingsMenu.firstPrint){
    settingsMenu.firstPrint = false;
  }

  if (enc.isLeft()) {
    settingsMenu.currentItem = constrain(settingsMenu.currentItem - 1, -1, settingsMenu.itemsCount - 1);
    if(settingsMenu.currentItem < 12) {
      currentMenuPage = 0;
    }
    else {
      currentMenuPage = 1;
    }
    settingsMenu.update = true;
  }
  else if (enc.isRight()) {
    settingsMenu.currentItem = constrain(settingsMenu.currentItem + 1, -1, settingsMenu.itemsCount - 1);
    if(settingsMenu.currentItem < 12) {
      currentMenuPage = 0;
    }
    else {
      currentMenuPage = 1;
    }
    settingsMenu.update = true;
  }
  else if (enc.isLeftH()) {
    if(settingsMenu.currentItem != -1 && settingsMenuItems[settingsMenu.currentItem].hasParams()) {
      settingsParams[settingsMenu.currentItem] = 
          settingsMenuItems[settingsMenu.currentItem].decParamsValue(settingsParams[settingsMenu.currentItem]);
      
      settingsMenu.update = true;

      if(settingsMenu.currentItem <= 5) {
        steppers[0]._stepsPerDeg = 
            getStepsPerRev(settingsParams[0], settingsParams[1], settingsParams[2], (bool)settingsParams[3], (bool)settingsParams[4], (bool)settingsParams[5]) / 360.0;
        steppers[0].reset();
      }
      else if(settingsMenu.currentItem <= 11) {
        steppers[1]._stepsPerDeg = 
            getStepsPerRev(settingsParams[6], settingsParams[7], settingsParams[8], (bool)settingsParams[9], (bool)settingsParams[10], (bool)settingsParams[11]) / 360.0;
        steppers[1].reset();
      }
    }
  }
  else if (enc.isRightH()) {
    if(settingsMenu.currentItem != -1 && settingsMenuItems[settingsMenu.currentItem].hasParams()) {
      settingsParams[settingsMenu.currentItem] = 
          settingsMenuItems[settingsMenu.currentItem].incParamsValue(settingsParams[settingsMenu.currentItem]);
      
      settingsMenu.update = true;

      if(settingsMenu.currentItem <= 5) {
        steppers[0]._stepsPerDeg = 
            getStepsPerRev(settingsParams[0], settingsParams[1], settingsParams[2], (bool)settingsParams[3], (bool)settingsParams[4], (bool)settingsParams[5]) / 360.0;
        steppers[0].reset();
      }
      else if(settingsMenu.currentItem <= 11) {
        steppers[1]._stepsPerDeg = 
            getStepsPerRev(settingsParams[6], settingsParams[7], settingsParams[8], (bool)settingsParams[9], (bool)settingsParams[10], (bool)settingsParams[11]) / 360.0;
        steppers[1].reset();
      }
    }
  }

  if (enc.hasClicks()) {
    switch(enc.clicks) {
      case 1:
        if(settingsMenu.currentItem == -1) {     
          currentPage = MENU_PAGE;
          
          disableInterrupts();
          EEPROM.put(SETTINGS_PARAMS_EEPROM_ADDRESS, settingsParams);
          enableInterrupts();
        }
        break;
    }
  }

  if (enc.isHolded()) {
    currentPage = MENU_PAGE;
    
    disableInterrupts();
    EEPROM.put(SETTINGS_PARAMS_EEPROM_ADDRESS, settingsParams);
    enableInterrupts();
  }

  if(settingsMenu.update) {
    if(prevMenuPage != currentMenuPage) {
      lcd.clear();
      lcdPrintCenter(settingsMenu.name, 0);
      settingsMenu.firstPrint = true;
      prevMenuPage = currentMenuPage;
    }
    
    if(currentMenuPage == 0) {
      lcdPrint(settingsMenuItems[0].name + (String)settingsParams[0], 1, 1, 9);
      lcdPrint(settingsMenuItems[1].name + (String)settingsParams[1], 1, 2, 4);
      lcdPrint(settingsMenuItems[2].name + (String)settingsParams[2], 6, 2, 4);
      lcdPrint(settingsMenuItems[3].name, 1, 3, 1);
      (bool)settingsParams[3] ? lcdPrintChar(BLOCK_CHAR, 2, 3) : lcdPrintChar(EMPTY_BLOCK_CHAR, 2, 3);
      lcdPrint(settingsMenuItems[4].name, 4, 3, 1);
      (bool)settingsParams[4] ? lcdPrintChar(BLOCK_CHAR, 5, 3) : lcdPrintChar(EMPTY_BLOCK_CHAR, 5, 3);
      lcdPrint(settingsMenuItems[5].name, 7, 3, 1);
      (bool)settingsParams[5] ? lcdPrintChar(BLOCK_CHAR, 8, 3) : lcdPrintChar(EMPTY_BLOCK_CHAR, 8, 3);
      lcdPrint(settingsMenuItems[6].name + (String)settingsParams[6], 11, 1, 9);
      lcdPrint(settingsMenuItems[7].name + (String)settingsParams[7], 11, 2, 4);
      lcdPrint(settingsMenuItems[8].name + (String)settingsParams[8], 16, 2, 4);
      lcdPrint(settingsMenuItems[9].name, 11, 3, 1);
      (bool)settingsParams[9] ? lcdPrintChar(BLOCK_CHAR, 12, 3) : lcdPrintChar(EMPTY_BLOCK_CHAR, 12, 3);
      lcdPrint(settingsMenuItems[10].name, 14, 3, 1);
      (bool)settingsParams[10] ? lcdPrintChar(BLOCK_CHAR, 15, 3) : lcdPrintChar(EMPTY_BLOCK_CHAR, 15, 3);
      lcdPrint(settingsMenuItems[11].name, 17, 3, 1);
      (bool)settingsParams[11] ? lcdPrintChar(BLOCK_CHAR, 18, 3) : lcdPrintChar(EMPTY_BLOCK_CHAR, 18, 3);
    }
    else {
      lcdPrint(settingsMenuItems[12].name, 1, 1, 9);
      (bool)settingsParams[12] ? lcdPrintChar(BLOCK_CHAR, 9, 1) : lcdPrintChar(EMPTY_BLOCK_CHAR, 9, 1);
      lcdPrint(settingsMenuItems[13].name, 11, 1, 9);
      (bool)settingsParams[13] ? lcdPrintChar(BLOCK_CHAR, 19, 1) : lcdPrintChar(EMPTY_BLOCK_CHAR, 19, 1);
      lcdPrint(settingsMenuItems[14].name + (String)settingsParams[14], 1, 2, 13);
      lcdPrint("deg/s", 15, 2);
      lcdPrint(settingsMenuItems[15].name + (String)settingsParams[15], 1, 3, 13);
      lcdPrint("deg/s", 15, 3);
    }
    
    settingsMenu.update = false;
  }

  switch(settingsMenu.currentItem) {
    case -1:
      printPointer(19, 0, settingsMenu.firstPrint, BACK_CHAR);
      break;
    case 0:
      printPointer(0, 1, settingsMenu.firstPrint);
      break;
    case 1:
      printPointer(0, 2, settingsMenu.firstPrint);
      break;
    case 2:
      printPointer(5, 2, settingsMenu.firstPrint);
      break;
    case 3:
      printPointer(0, 3, settingsMenu.firstPrint);
      break;
    case 4:
      printPointer(3, 3, settingsMenu.firstPrint);
      break;
    case 5:
      printPointer(6, 3, settingsMenu.firstPrint);
      break;
    case 6:
      printPointer(10, 1, settingsMenu.firstPrint);
      break;
    case 7:
      printPointer(10, 2, settingsMenu.firstPrint);
      break;
    case 8:
      printPointer(15, 2, settingsMenu.firstPrint);
      break;
    case 9:
      printPointer(10, 3, settingsMenu.firstPrint);
      break;
    case 10:
      printPointer(13, 3, settingsMenu.firstPrint);
      break;
    case 11:
      printPointer(16, 3, settingsMenu.firstPrint);
      break;
    case 12:
      printPointer(0, 1, settingsMenu.firstPrint);
      break;
    case 13:
      printPointer(10, 1, settingsMenu.firstPrint);
      break;
    case 14:
      printPointer(0, 2, settingsMenu.firstPrint);
      break;
    case 15:
      printPointer(0, 3, settingsMenu.firstPrint);
      break;
  }
}

void displayAboutPage(bool forceClear = false) {
  if (lastDisplayedPage != currentPage || forceClear) {
    lastDisplayedPage = currentPage;
    lcd.clear();
    lcdPrintCenter("Created by", 1);
    lcdPrintCenter("Maxim Kliuba", 2);
    printPointer(19, 0, true, BACK_CHAR);
  }

  if (enc.isHolded()) {
    currentPage = MENU_PAGE;
  }

  if (enc.hasClicks()) {
    switch(enc.clicks) {
      case 1:
        currentPage = MENU_PAGE;
        break;
    }
  }
}

void lcdPrint(const String text, byte x, byte y, byte maxLength = 0) {
  lcd.setCursor(x, y);
  lcd.print(text);
  byte length = text.length();
  for(byte i = 0; i < maxLength - length; i++) {
    lcd.write(SPACE_CHAR);
  } 
}

void lcdPrintCenter(const String text, byte y) {
  lcd.setCursor(0, y);
  const byte x1 = (LCD_COLS - text.length()) / 2;
  for(byte i = 0; i < x1; i++) {
    lcd.write(SPACE_CHAR);
  }
  lcd.print(text);
  const byte x2 = LCD_COLS - (x1 + text.length());
  for(byte i = 0; i < x2; i++) {
    lcd.write(SPACE_CHAR);
  }
}

void printPointer(byte x, byte y, bool firstPrint = false, byte charCode = POINTER_CHAR) {
  static byte _prevPositionX = 0;
  static byte _prevPositionY = 0;

  if(firstPrint || _prevPositionX != x || _prevPositionY != y) {
    if(!firstPrint) {
      lcdPrintChar(SPACE_CHAR, _prevPositionX, _prevPositionY);
    }

    _prevPositionX = x;
    _prevPositionY = y;

    lcdPrintChar(charCode, x, y);
  }
}

void lcdPrintChar(byte charCode, byte x, byte y) {
  lcd.setCursor(x, y);
  lcd.write(charCode);
}

String formatTime(unsigned long time) {
  return str(hour((time_t)time)) + ":" + str(minute((time_t)time)) + ":" + str(second((time_t)time));
}

String str(int num) {
  String str = (String)num;
  return (str.length() == 1 ? "0" + str : str);
}

void startCurrentMode(int mode) {
  time = 0;
  for(byte i = 0; i < 4; i++) {
    recipeStopParams[i] = 0;
  }
  info.currentMode = mode;
  info.state = RUN;
}

void goToStartPoint() {
  int deg[2];
  byte whileFlag[2] = { 0, 0, };
  
  for(byte i = 0; i < 2; i++) {
    if((bool)settingsParams[12 + i]) {
      steppers[i].setRunMode(KEEP_SPEED);
      setStepperSpeedDeg(i, settingsParams[15]);
      deg[i] = (int)steppers[i].getCurrentDeg();
      whileFlag[i] = 1;
    }
    else {
      steppers[i].setRunMode(FOLLOW_POS);
      setStepperSpeedDeg(i, settingsParams[15]);
      deg[i] = ceil(steppers[i].getCurrentDeg() / 360.0) * 360;
      steppers[i].setTargetDeg(deg[i]);
      whileFlag[i] = 2;
    }
  }

  while(whileFlag[0] != 0 || whileFlag[1] != 0) {
    if((whileFlag[0] == 1 && (magneticSensors[0].state() || abs((int)steppers[0].getCurrentDeg() - deg[0]) >= 360 * 3)) || (whileFlag[0] == 2 && !steppers[0].tick())) {
      steppers[0].reset();
      whileFlag[0] = 0;
    }
    if((whileFlag[1] == 1 && (magneticSensors[1].state() || abs((int)steppers[1].getCurrentDeg() - deg[1]) >= 360 * 3)) || (whileFlag[1] == 2 && !steppers[1].tick())) {
      steppers[1].reset();
      whileFlag[1] = 0;
    }
    delay(10);
  }
}

void resetMotors() {
  int deg[2];
  byte whileFlag[2] = { 0, 0, };
  
  for(byte i = 0; i < 2; i++) {
    if((bool)settingsParams[12 + i]) {
      steppers[i].setRunMode(KEEP_SPEED);
      setStepperSpeedDeg(i, settingsParams[15]);
      deg[i] = (int)steppers[i].getCurrentDeg();
      whileFlag[i] = 1;
    }
    else {
      whileFlag[i] = 2;
    }
  }

  while(whileFlag[0] != 0 || whileFlag[1] != 0) {
    if((whileFlag[0] == 1 && (magneticSensors[0].state() || abs((int)steppers[0].getCurrentDeg() - deg[0]) >= 360 * 3)) || whileFlag[0] == 2) {
      steppers[0].reset();
      whileFlag[0] = 0;
    }
    if((whileFlag[1] == 1 && (magneticSensors[1].state() || abs((int)steppers[1].getCurrentDeg() - deg[1]) >= 360 * 3)) || whileFlag[1] == 2) {
      steppers[1].reset();
      whileFlag[1] = 0;
    }
    delay(10);
  }
}

int getStepsPerRev(int spr, int s, int g, bool ms1, bool ms2, bool ms3) {
  int stepsPerRev = spr * (float)g / s;
        
  if(!ms1 && !ms2 && !ms3) {
    stepsPerRev *= 1;
  }
  else if(ms1 && !ms2 && !ms3) {
    stepsPerRev *= 2;
  }
  else if(!ms1 && ms2 * !ms3) {
    stepsPerRev *= 4;
  }
  else if(ms1 && ms2 && !ms3) {
    stepsPerRev *= 8;
  }
  else if(ms1 && ms2 && ms3) {
    stepsPerRev *= 16;
  }

  return stepsPerRev;
}

void setStepperSpeedDeg(byte i, int speed) {
  Timer2.disableISR(CHANNEL_A); 
  steppers[i].setMaxSpeedDeg(speed);
  steppers[i].setAccelerationDeg(0);
  steppers[i].setSpeedDeg(speed);
  Timer2.setPeriod(steppers[i].getMinPeriod());
  Timer2.enableISR(CHANNEL_A);
  isTimer2aEnadle = true;
}

int getParamsSpeedDeg() {
  if(info.currentMode != -1) {
    return constrain(
        map(
            recipeParams[info.currentMode][1], 
            recipeMenuItems[1].getMinParamsValue(), 
            recipeMenuItems[1].getMaxParamsValue(), 
            settingsParams[14], 
            settingsParams[15]
        ), 
        settingsParams[14], 
        settingsParams[15]
      );
  }  
  return 0;
}

ISR(TIMER2_A) {
  for(byte i = 0; i < (sizeof(steppers) / sizeof(*steppers)); i++) {
    steppers[i].tick();
    magneticSensors[i].tick();
  }
}

void encButtonInterrupt() {
  enc.tick();
}

void disableInterrupts() {
  if(isTimer2aEnadle) {
    Timer2.pause(); 
  }
  detachInterrupt(digitalPinToInterrupt(ENCODER_KEY_PIN));
  detachInterrupt(digitalPinToInterrupt(ENCODER_S2_PIN));
}

void enableInterrupts() {
  attachInterrupt(digitalPinToInterrupt(ENCODER_KEY_PIN), encButtonInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_S2_PIN), encButtonInterrupt, CHANGE);
  if(isTimer2aEnadle) {
    Timer2.resume(); 
  }
}

#include <Arduino.h>
#include <ezOutput.h>
#include <LiquidCrystal.h>
#include <Wire.h>

// LCD
#define LCD_RS_PIN 2
#define LCD_ENA_PIN 3
#define LCD_D4_PIN 4
#define LCD_D5_PIN 5
#define LCD_D6_PIN 6
#define LCD_D7_PIN 7
#define LCD_A_PIN 8

// LED Diodes
#define LED1_PIN 9
#define LED2_PIN 10
#define LED3_PIN 11
#define LED4_PIN 12

// Light Dependent Resistors (LDR)
#define LDR1_PIN A0
#define LDR2_PIN A1
#define LDR3_PIN A2
#define LDR4_PIN A3

// Push Buttons
#define BTN1_PIN 26 // Start & Restart
#define BTN2_PIN 27 // Resume & Pause
#define BTN3_PIN 28 // Stop

// Buzzer
#define BUZZER_PIN 52

ezOutput LED1(LED1_PIN);
ezOutput LED2(LED2_PIN);
ezOutput LED3(LED3_PIN);
ezOutput LED4(LED4_PIN);

LiquidCrystal lcd(
  LCD_RS_PIN,
  LCD_ENA_PIN, 
  LCD_D4_PIN, 
  LCD_D5_PIN, 
  LCD_D6_PIN, 
  LCD_D7_PIN
);

enum State {
  ERROR,
  CONTINUE,
  PAUSE,
  RESTART,
  SLEEP,
  START,
  FINISHED,
  STOP
};

const int darknessThreshold = 360;
const int luminosityThreshold = 600;
const int continueDelay = 16000;
const int debounceDelay = 250;
const int printDelay = 150;
const int sleepDelay = 2500;
const int turnonDelay = 25;

unsigned long previousInputMillis;
unsigned long previousLCDMillis;
unsigned long previousStartMillis;
unsigned long previousStateMillis;

unsigned long lastLEDMillis;
State currentState;
String errorMessage;
int lastLED;

void setState(State newState) {
  previousStateMillis = millis();
  currentState = newState;
}

void setup () {
  Serial.begin(9600);

  pinMode(LCD_RS_PIN, OUTPUT);
  pinMode(LCD_ENA_PIN, OUTPUT);
  pinMode(LCD_D4_PIN, OUTPUT);
  pinMode(LCD_D5_PIN, OUTPUT);
  pinMode(LCD_D6_PIN, OUTPUT);
  pinMode(LCD_D7_PIN, OUTPUT);
  pinMode(LCD_A_PIN, OUTPUT);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);

  pinMode(LDR1_PIN, INPUT);
  pinMode(LDR2_PIN, INPUT);
  pinMode(LDR3_PIN, INPUT);
  pinMode(LDR4_PIN, INPUT);

  pinMode(BTN1_PIN, INPUT);
  pinMode(BTN2_PIN, INPUT);
  pinMode(BTN3_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.noCursor();
  
  setState(State::START);
}

void changeState(State newState) {
  
  if(currentState == State::ERROR && newState != State::ERROR) {
    digitalWrite(BUZZER_PIN, LOW);
    setState(newState);
  } else {
    setState(newState);
  }
}

void readInput() {
  bool btn1 = digitalRead(BTN1_PIN);
  bool btn2 = digitalRead(BTN2_PIN);
  bool btn3 = digitalRead(BTN3_PIN);

  if(btn1 ^ btn2 ^ btn3) {
    unsigned long currentMillis = millis();

    if(currentMillis - previousInputMillis > debounceDelay) {
      previousInputMillis = currentMillis;

      if(btn3) {
        if(currentState == State::STOP || currentState == State::START || currentState == State::FINISHED)
          changeState(State::SLEEP);
        else if(currentState != State::SLEEP){
          changeState(State::STOP);
        }
      } else if(btn2) {
        if(currentState == State::PAUSE || currentState == State::START || currentState == State::FINISHED) {
          changeState(State::CONTINUE);
        } else {
          changeState(State::PAUSE);
        }
      } else if(btn1) {
        if(currentState == State::START) {
          changeState(State::CONTINUE);
        } else if(currentState == State::FINISHED || currentState == State::STOP) {
          changeState(State::RESTART);
        } else {
          changeState(State::START);
        }
      }
    }
  }

}

void setupLEDs() {
  LED1.pulse(2000);
  LED2.pulse(2000, 4000);
  LED3.pulse(2000, 8000);
  LED4.pulse(2000, 12000);
  previousStartMillis = millis();
}

void clear() {
  lcd.clear();
  lcd.home();
}

void print(String firstLine, String secondLine) {
  unsigned long currentMillis = millis();

  if(currentMillis - previousLCDMillis > printDelay) {
    previousLCDMillis = currentMillis;
    
    clear();
    lcd.print(firstLine);
    lcd.setCursor(0, 1);
    lcd.print(secondLine);
  }

}

void checkLED(ezOutput output, String name, int sensorPin, int outputPin) {
  if(output.getState()) {
    unsigned long currentMillis = millis();

    if(lastLED != outputPin)
      lastLEDMillis = currentMillis;

    lastLED = outputPin;    

    if(currentMillis - lastLEDMillis > turnonDelay) {
      int resistance = analogRead(sensorPin);
      
      if(resistance > luminosityThreshold) {
        print(name + " is high!", "");
      } else {
        errorMessage = "Error in " + name + "!";
        changeState(State::ERROR);
      }
    }

  } else if(lastLED == outputPin) {
    print(name + " is low!", "");
  }
}

void onContinue() {
  unsigned long currentMillis = millis();

  LED1.loop();
  LED2.loop();
  LED3.loop();
  LED4.loop();

  checkLED(LED1, "LED1", LDR1_PIN, LED1_PIN);
  checkLED(LED2, "LED2", LDR2_PIN, LED2_PIN);
  checkLED(LED3, "LED3", LDR3_PIN, LED3_PIN);
  checkLED(LED4, "LED4", LDR4_PIN, LED4_PIN);

  if(currentMillis - previousStartMillis > continueDelay) {
    changeState(State::FINISHED);
  }

}

void onError() {
  print(errorMessage, "Press start...");
  LED1.low();
  LED2.low();
  LED3.low();
  LED4.low();

  digitalWrite(BUZZER_PIN, HIGH);
}

void onFinished() {
  print("Finished!", "Press start...");
}

void onPause() {
  print("Paused!", "Press play...");
}

void onRestart() {
  setupLEDs();
  changeState(State::CONTINUE);
}

void onSleep() {
  unsigned long currentMillis = millis();
  
  print("Falling asleep!", "   zzz...");

  if(currentMillis - previousStateMillis > sleepDelay) {
    clear();
    delay(printDelay);
    digitalWrite(LCD_A_PIN, LOW);
  }

}

void onStart() {
  setupLEDs();
  digitalWrite(LCD_A_PIN, HIGH);
  print("We are ready,", "press start...");
}

void onStop() {
  print("Stopped!", "Press start...");
  LED1.low();
  LED2.low();
  LED3.low();
  LED4.low();  
}

void loop() {
  readInput();

  switch (currentState) {
  case CONTINUE:
    onContinue();
    break;
  case ERROR:
    onError();
    break;
  case FINISHED:
    onFinished();
    break;
  case PAUSE:
    onPause();
    break;
  case RESTART:
    onRestart();
    break;
  case SLEEP:
    onSleep();
    break;
  case START:
    onStart();
    break;
  case STOP:
    onStop();
    break;
  default:
    break;
  }
}
/**
  Hydroponics system, uses :
  - HC-SR04 ultrasound sensor
  - LCD I2C TWI LCD2004 Module (SDA : pin 2 / SCL : pin 3)
  - G1/2 Water Flow sensor (Model:POW110D3B)
**/

#include "Params.h"

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Process.h>
#include <NewPing.h> // For ultrasonic sensor

long lastTimeWaterRead, lastTimePumpCheck;
long curTime;
float waterVol = 0; // in liters
boolean isPumpOn = false;
long lastTimeNotPumpOk=0, lastTimeNotWaterLevelOk=0;

int waterHFreq, pumpOkFreq;
long notPumpOkNotifDelay, notWaterOkNotifDelay;

volatile int waterFlowCount;
String mailCommand;

// Set the LCD address to 0x20 for a 20 chars and 4 line display
LiquidCrystal_I2C lcd(0x20, 20, 4);

// Ultrasonic init
NewPing sonar(USND_TRIG_PIN, USND_ECHO_PIN, USND_MAX_DIST);

// Interrupt called function
void countWaterFlow() {
  waterFlowCount++;
}

void setup() {
  mailCommand = MAIL_BIN " " SMTP_SERVER;
  mailCommand += " " SMTP_USERNAME;
  mailCommand += " " SMTP_PASSWORD;
  mailCommand += " " SMTP_FROM;
  mailCommand += " " SMTP_TO;
  waterHFreq = WATER_H_FREQ;
  pumpOkFreq = PUMP_OK_FREQ;
  notWaterOkNotifDelay = NOT_WATER_OK_NOTIF;
  notPumpOkNotifDelay = NOT_PUMP_OK_NOTIF;

  Bridge.begin();
  lcd.init();
  lcd.backlight();

  pinMode(USND_TRIG_PIN, OUTPUT);
  pinMode(USND_ECHO_PIN, INPUT);
  pinMode(WATER_FLOW_PIN, INPUT);

  lastTimeWaterRead = millis();
  lastTimePumpCheck = millis();
  if (DEBUG) {
    waterHFreq = 1000;
    pumpOkFreq = 1000;
    notWaterOkNotifDelay = 5000;
    notPumpOkNotifDelay = 5000;
  }
  lcdClearLine(0);
  lcd.print("Setup done...");
  sendEmail("Arduino Hydroponics is up!");
}

void loop() {
  if (millis() - lastTimePumpCheck >= pumpOkFreq) {
    checkWaterPump();
    lastTimePumpCheck = millis();
  }

  if (millis() - lastTimeWaterRead >= waterHFreq) {
    readWaterVolume();
    lastTimeWaterRead = millis();
  }
}

void checkWaterPump() {
  waterFlowCount = 0;
  // Enable interrupts
  attachInterrupt(WATER_FLOW_ITR, countWaterFlow, RISING);
  delay(1000);
  // Disable interrupts
  detachInterrupt(WATER_FLOW_ITR);
  isPumpOn = (waterFlowCount >= WATER_WHEEL_MIN_PULSE);
  if (!isPumpOn) {
    if (lastTimeNotPumpOk == 0 || ((millis() - lastTimeNotPumpOk) >= notPumpOkNotifDelay)) {
      if (sendEmail("The pump is off")) 
        lastTimeNotPumpOk = millis();
      }
  }
  displaySystemInfo();
}

void readWaterVolume() {
  // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
  delay(50);
  
  // Do multiple pings (default=5), discard out of range pings and return median in microseconds
  unsigned int distance = sonar.convert_cm(sonar.ping_median());
  waterVol = ((TANK_HEIGHT - distance) * TANK_SURFACE) / 1000;
  if (waterVol < TANK_MIN_L || waterVol > TANK_MAX_L) {
    if (lastTimeNotWaterLevelOk == 0 || ((millis() - lastTimeNotWaterLevelOk) >= notWaterOkNotifDelay)) {
      if (sendEmail("Water volume is wrong : " + String(waterVol) + " L")) {
        lastTimeNotWaterLevelOk = millis();
      }
    }
  }
  else {
    if (isNotifiedWaterWrong) {
      sendEmail("Water volume is now OK : " + String(waterVol) + " L");
    }    
    isNotifiedWaterWrong = false;
  }
  
  displayWaterInfo();
  if (DEBUG) {
    String d = "d wat.=" + String(distance) + " " + String(waterVol) + " L";
    displayDebug(d);
  }
}

void displaySystemInfo() {
  lcdClearLine(0);
  String v = isPumpOn ? "Pump:ON" : "PUMP:OFF";
  v += DEBUG ? " Debug:ON" : "";
  lcd.print(v);
}

void displayWaterInfo() {
  lcdClearLine(1);
  char qty[6]="ERR";
  
  if (waterVol >= 0 && waterVol <= TANK_MAX_L) {
    dtostrf(waterVol, 4, 2, qty);
  }

  String s = "W=" + String(qty) + "L";
  lcd.print(s);
}

boolean sendEmail(String msg) {
  if (SEND_MAIL) {
    Process p;
    String email = mailCommand + " \"[Hydroponics] " + msg + "\" \"" + msg + "\"";
    //mailCommand += " \"[Hydroponics] " + msg + "\" \"" + msg + "\"";
    displayDebug("Sending mail...");
    int res = p.runShellCommand(email);
    delay(2000);
    return (res == 0);
  }
  else {
    return true;
  }
}

void lcdClearLine(int lineNum) {
  lcd.setCursor(0, lineNum);
  lcd.print("                    ");
  lcd.setCursor(0, lineNum);
}

void displayDebug(String s) {
  if (DEBUG) {
    lcdClearLine(3);
    lcd.print(s);
  }
}


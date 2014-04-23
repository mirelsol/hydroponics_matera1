/**
  Hydroponics system, uses :
  - HC-SR04 ultrasound sensor
  - LCD I2C TWI LCD2004 Module (SDA : pin 2 / SCL : pin 3)
  - Analog pH meter SEN0161
  - G1/2 Water Flow sensor (Model:POW110D3B)
**/

#include "Params.h"

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Process.h>

long lastTimeWaterRead, lastTimePhRead, lastTimePumpCheck;
long curTime;
float waterVol = 0; // in liters
float phValue = 0;
boolean isPumpOn = false;
boolean isNotifiedPumpOff = false;
boolean isNotifiedPhWrong = false;
boolean isNotifiedWaterWrong = false;
int waterHFreq, pumpOkFreq, phFreq;

volatile int waterFlowCount;
String mailCommand;

// Set the LCD address to 0x20 for a 20 chars and 4 line display
LiquidCrystal_I2C lcd(0x20, 20, 4);

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
  phFreq = PH_FREQ;

  if (DEBUG) {
    Serial.begin(9600);
  }
  Bridge.begin();
  lcd.init();
  lcd.backlight();

  pinMode(USND_TRIG_PIN, OUTPUT);
  pinMode(USND_ECHO_PIN, INPUT);
  pinMode(WATER_FLOW_PIN, INPUT);

  lastTimeWaterRead, lastTimePhRead, lastTimePumpCheck = millis();
  if (DEBUG) {
    waterHFreq = 1000;
    pumpOkFreq = 1000;
    phFreq = 1000;
  }
  lcdClearLine(0);
  lcd.print("Setup done...");
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

  if (millis() - lastTimePhRead >= phFreq) {
    readPh();
    lastTimePhRead = millis();
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
    if (!isNotifiedPumpOff) { // Just send a notification once
      if (sendEmail("The pump is off")) {
        isNotifiedPumpOff = true;
      }
    }
  }
  else {
    isNotifiedPumpOff = false;
  }
  displaySystemInfo();
}

void readWaterVolume() {
  // The following trigPin/echoPin cycle is used to determine the
  // distance of the nearest object by bouncing soundwaves off of it.
  // Test distance = (high level time×velocity of sound (340M/S) / 2
  digitalWrite(USND_TRIG_PIN, LOW); 
  delayMicroseconds(2); 
  
  digitalWrite(USND_TRIG_PIN, HIGH);
  delayMicroseconds(10); 
  
  digitalWrite(USND_TRIG_PIN, LOW);
  // pulseIn waits for the pin to go HIGH, starts timing, then waits for the pin to go LOW and stops timing
  long duration = pulseIn(USND_ECHO_PIN, HIGH);
  
  // Calculate the distance (in cm) based on the speed of sound.
  //distance = duration/58.2;
  //float distance = round(duration * 0.0340 / 2); // 5 mm error tolerance
  float distance = duration * 0.0340 / 2;
  waterVol = ((TANK_HEIGHT - round(distance)) * TANK_SURFACE) / 1000;
  if (waterVol < TANK_MIN_L || waterVol > TANK_MAX_L) {
    if (!isNotifiedWaterWrong) { // Just send a notification once
      if (sendEmail("Water volume is wrong : " + String(waterVol) + " L")) {
        isNotifiedWaterWrong = true;
      }
    }
  }
  else {
    isNotifiedWaterWrong = false;
  }
  
  displayWaterInfo();
  if (DEBUG) {
    char v[6];
    dtostrf(distance, 4, 2, v);
    String d = "d wat.=" + String(v);
    displayDebug(d);
  }
}

void readPh() {
  int buf[10];
  // Get 10 sample value from the sensor for smooth the value  
  for (int i=0; i<10; i++) { 
    buf[i] = analogRead(PH_PIN);
    delay(10);
  }
  // Sort the analog from small to large
  for (int i=0; i<9; i++) {
    for(int j=i+1; j<10; j++) {
      if(buf[i]>buf[j]) {
        int temp=buf[i];
        buf[i]=buf[j];
        buf[j]=temp;
      }
    }
  }
  
  // Take the average value of 6 center sample
  unsigned long avgValue = 0;
  for(int i=2; i<8; i++) {
    avgValue += buf[i];
  }
  // --- convert the analog into millivolt
  phValue = (float)avgValue * 5.0 / 1024 / 6;
  // --- convert the millivolt into pH value
  // pH = -0.017 * mV + 7 + PH_OFFSET
  phValue = 3.5 * phValue + PH_OFFSET;
  
  if (phValue < PH_MIN || phValue > PH_MAX) {
    if (!isNotifiedPhWrong) { // Just send it once
      if (sendEmail("Ph value wrong : " + String(phValue))) {
        isNotifiedPhWrong = true;
      }
    }
  }
  else {
    isNotifiedPhWrong = false;
  }
  
  displayWaterInfo();
}

void displaySystemInfo() {
  lcdClearLine(0);
  String v = isPumpOn ? "Pump:ON" : "PUMP:OFF";
  v += DEBUG ? " Debug:ON" : "";
  lcd.print(v);
}

void displayWaterInfo() {
  lcdClearLine(1);
  char qty[6]="ERR", ph[6];
  if (waterVol >= 0 && waterVol <= TANK_MAX_L) {
    dtostrf(waterVol, 4, 2, qty);
  }
  dtostrf(phValue, 4, 2, ph);
  String s = "W=" + String(qty) + "L - pH=" + String(ph);
  lcd.print(s);
}

boolean sendEmail(String msg) {
  if (SEND_MAIL) {
    Process p;
    mailCommand += " \"[Hydroponics] " + msg + "\" \"" + msg + "\"";
    displayDebug("Sending mail...");
    int res = p.runShellCommand(mailCommand);
    // do nothing until the process finishes, so you get the whole output:
    //while(p.running());  
    displayDebug("Email sent");
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


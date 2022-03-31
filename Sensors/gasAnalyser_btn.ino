#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <MQUnifiedsensor.h>
#include "EspMQTTClient.h"

#define LEDRED D0
#define LEDGREEN D5
#define LEDBLUE D6
#define BTNPIN 13   // pin D7

unsigned long lastBlink;
bool secondVisible = 1;
bool btnLast = LOW;
bool btnCurrent = LOW;
bool screenPos = 0;

// Клиент MQTT
EspMQTTClient client(
  "hamann",
  "Dl34pmkU",
  "192.168.31.247",  // MQTT Broker server ip
  "hamann",   // Can be omitted if not needed
  "Dl34pmkU",   // Can be omitted if not needed
  "WemosKitchen",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

// Дисплей OLED SSD1306 128x64
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     D4
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Часы реального времени
RTC_DS1307 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Датчик MQ-9
#define Board              ("ESP8266")
#define Pin                (A0)
#define Type               ("MQ-9")
#define Voltage_Resolution (5)
#define ADC_Bit_Resolution (10)
#define RatioMQ9CleanAir   (9.6) 
MQUnifiedsensor MQ9(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);

void setup() {
  Serial.begin(9600);

// Инициализация дисплея OLED SSD1306
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
    
// Инициализация кнопки и RGB-светодиода  
  pinMode(BTNPIN, INPUT);
  pinMode(LEDRED, OUTPUT);
  pinMode(LEDGREEN, OUTPUT);
  pinMode(LEDBLUE, OUTPUT);
  digitalWrite(LEDRED, LOW);
  digitalWrite(LEDGREEN, LOW);
  digitalWrite(LEDBLUE, LOW);

// Инициализация RTC DS1307
#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

// Инициализация датчика MQ-9
  MQ9.setRegressionMethod(1);
  MQ9.init(); 
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ9.update(); // Update data, the arduino will be read the voltage on the analog pin
    calcR0 += MQ9.calibrate(RatioMQ9CleanAir);
    Serial.print(".");
  }
  MQ9.setR0(calcR0/10);
  Serial.println("  done!.");
  
  display.display();
  delay(2000);
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished() {
}

void loop() {
  btnCurrent = digitalRead(BTNPIN);
  if ((btnCurrent == HIGH)) {
    screenPos = !screenPos;
  }
 
// Получаем данные с датччика MQ-9  
  MQ9.update();
  MQ9.setA(1000.5); MQ9.setB(-2.186);
  float LPG = MQ9.readSensor();
  MQ9.setA(4269.6); MQ9.setB(-2.648);
  float CH4 = MQ9.readSensor();
  MQ9.setA(599.65); MQ9.setB(-2.244);
  float CO = MQ9.readSensor();
// Индикация светодиодом
  if ((LPG < 40) && (CH4 < 50) && (CO < 35)) {
    digitalWrite(LEDRED, LOW);
    digitalWrite(LEDGREEN, HIGH);
    digitalWrite(LEDBLUE, LOW);
  } else {
    digitalWrite(LEDRED, HIGH);
    digitalWrite(LEDGREEN, LOW);
    digitalWrite(LEDBLUE, LOW);
  }

  DateTime now = rtc.now();

// Вывод даты на дисплей
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print(now.year());
  display.print("-");
  if (now.month() < 10) {
    display.print("0");
    display.print(now.month());
  } else {
    display.print(now.month());
  }
  display.print("-");
  if (now.day() < 10) {
    display.print("0");
    display.print(now.day());
  } else {
     display.print(now.day());
  }
  display.setCursor(0,9);
  display.print(daysOfTheWeek[now.dayOfTheWeek()]);

// Вывод времени на дисплей
  display.setTextSize(2);
  display.setCursor(65,0);
  if (now.hour() < 10) {
    display.print("0");
    display.print(now.hour());
  } else {
    display.print(now.hour());
  }
  lastBlink = millis();
  if (secondVisible == 1 && (millis() - lastBlink < 1000)) {
    display.print(":");
    secondVisible = 0;
  } else {
    display.print(" ");
    secondVisible = 1;
  }
  if (now.minute() < 10) {
    display.print("0");
    display.print(now.minute());
  } else {
    display.print(now.minute());
  }

  switch (screenPos) {
    case 0:
      // Вывод на дисплей концентрации газов
      display.setTextSize(1);
      display.setCursor(0,27);
      display.print("LPG: "); display.print(LPG);
      display.setCursor(50,27); display.print("   ppm");
      display.setCursor(0,42);
      display.print("CH4: "); display.print(CH4);
      display.setCursor(50,42); display.print("   ppm");
      display.setCursor(0,57);
      display.print("CO:  "); display.print(CO);
      display.setCursor(50,57); display.print("   ppm");
      break;
    case 1:
      display.setTextSize(1); display.setCursor(0,34); display.print("Temp: ");
      display.setTextSize(2); display.setCursor(40,27); display.print(LPG);
      display.setTextSize(1); display.setCursor(100,34); display.print((char)247); display.print("C");
      display.setTextSize(1); display.setCursor(0,55); display.print("Humi: ");
      display.setTextSize(2); display.setCursor(40,48); display.print(CO);
      display.setTextSize(1); display.setCursor(100,55); display.print("%");
      break;
  }
  display.display();
  
  client.publish("wemos/mq9/lpg", String(LPG));
  client.publish("wemos/mq9/ch4", String(CH4));
  client.publish("wemos/mq9/co", String(CO));
  client.publish("wemos/online", "Подключено");

//  Serial.println(LPG);
//  Serial.println(CH4);
//  Serial.println(CO);

  client.loop();
  delay(1000);
}

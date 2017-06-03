#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include "RTClib.h"
RTC_DS1307 RTC;


LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

const byte outPin = 2; // выход на реле
//подача HIGH на реле выключает (!) нагрузку
//LOW - включает


byte setMinClockOn; // 
byte setHorClockOn;
byte setMinClockOff; // 
byte setHorClockOff;

byte key(){ //// для кнопок ЛСДшилда
  int val = analogRead(0);
    if (val < 50) return 5;
    else if (val < 150) return 3;
    else if (val < 350) return 4;
    else if (val < 500) return 2;
    else if (val < 800) return 1;
    else return 0;  
}

void setClock(){ // установка часов
  byte pos = 1;
  char s0[16];
 
  DateTime now = RTC.now(); 
  
  lcd.clear();
  lcd.blink();

  while(key() != 1){ // крутим   цикл   
    byte KEY = key(); // читаем состояние кнопок
    delay(200);

      
    lcd.setCursor(1, 1);
    lcd.print("set to save");
    lcd.setCursor(0, 0);  
    sprintf (s0, "%02i:%02i %02i/%02i/%02i", now.hour(), now.minute(), now.day(), now.month(), now.year2());
    lcd.print(s0);
    
    lcd.setCursor(pos, 0); // устанавливаем курсор согласно позиции
    
    if (KEY == 5 && pos < 13) pos += 3; // крутим позицию
    else if (KEY == 2 && pos > 1) pos -= 3;
    
    else if (pos == 1 && KEY == 3) now.inc_hour(); // крутим значения
    else if (pos == 1 && KEY == 4) now.dec_hour();
    else if (pos == 4 && KEY == 3) now.inc_minute();
    else if (pos == 4 && KEY == 4) now.dec_minute();    
    else if (pos == 7 && KEY == 3) now.inc_day();
    else if (pos == 7 && KEY == 4) now.dec_day();    
    else if (pos == 10 && KEY == 3) now.inc_month();
    else if (pos == 10 && KEY == 4) now.dec_month();    
    else if (pos == 13 && KEY == 3) now.inc_year2();
    else if (pos == 13 && KEY == 4) now.dec_year2();  
  }
  
  RTC.adjust(now);
  lcd.noBlink(); 
  lcd.clear();
  lcd.print("     Saved");
  delay(1500);
}

void setOnOff(){    
  byte pos = 0;
  char s0[16];
  
  lcd.clear();
  lcd.blink();

  while(key() != 1){ // крутим   цикл   
    byte KEY = key(); // читаем состояние кнопок
      delay(200);
    lcd.setCursor(1, 1);
    lcd.print("set to save");
    lcd.setCursor(0, 0);
    sprintf (s0, "%02i:%02i %02i:%02i",setHorClockOn, setMinClockOn,setHorClockOff, setMinClockOff );
    lcd.print(s0);
      
    lcd.setCursor(pos, 0); // устанавливаем курсор согласно позиции
    
    if (KEY == 5 && pos < 9) pos += 3; // крутим позицию
    else if (KEY == 2 && pos > 1) pos -= 3;
    
    else if (pos == 0 && KEY == 3) setHorClockOn++; // крутим значения
    else if (pos == 0 && KEY == 4) setHorClockOn--;
    else if (pos == 3 && KEY == 3) setMinClockOn++;
    else if (pos == 3 && KEY == 4) setMinClockOn--;    
    else if (pos == 6 && KEY == 3) setHorClockOff++;
    else if (pos == 6 && KEY == 4) setHorClockOff--;    
    else if (pos == 9 && KEY == 3) setMinClockOff++;
    else if (pos == 9 && KEY == 4) setMinClockOff--;    
 
    
    if (setHorClockOn > 23) setHorClockOn = 0;
    else if (setMinClockOn > 59) setMinClockOn = 0;
    else if (setHorClockOff > 23) setHorClockOff = 0;
    else if (setMinClockOff > 59) setMinClockOff = 0;
    
  }// конец цикла
   lcd.noBlink(); 
   lcd.clear();

   EEPROM.write(0, setMinClockOn);
   EEPROM.write(1, setHorClockOn);
   EEPROM.write(3, setMinClockOff);
   EEPROM.write(4, setHorClockOff);

   lcd.print("     Saved");
   delay(1500);
}///
 
void menu(){
  lcd.clear();
  char menuTxt[2][14] = {"set ON/OFF >>", "set clock  >>"};
  byte pos = 0;
  
  while(1){  
    delay(200);  
    byte KEY = key();
    
    lcd.setCursor(0, 0);
    lcd.print(pos+1);
    lcd.print(".");
    lcd.print(menuTxt[pos]);
    
    if (KEY == 3 && pos != 0) pos--;
    else if (KEY == 4 && pos < 1) pos++;
    
    if (KEY == 5 && pos == 0) setOnOff();
    else if (KEY == 5 && pos == 1) setClock(); 
  }
}  
 
void setup(){
  //отладка 
  
  
  RTC.begin();
  
  Wire.begin(); 
  lcd.begin(16, 2);
  lcd.clear();
  
  pinMode(outPin, OUTPUT);
  digitalWrite(outPin, HIGH);
  
  setMinClockOn = EEPROM.read(0);
  setHorClockOn = EEPROM.read(1);
  setMinClockOff = EEPROM.read(3);
  setHorClockOff = EEPROM.read(4);
  
  if (! RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));

  }
}

void loop()
{
  
  char s0[16];
  int state = digitalRead(outPin);
  
  //byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;  
  //getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);

  DateTime now = RTC.now();
  
  // обработка кнопок
  if (key() == 1) menu(); // если нажата селект
  else if (key() == 3) digitalWrite(outPin, LOW);
  else if (key() == 4) digitalWrite(outPin, HIGH);


  // сравниваем время и управляем выходом// 
  //логика такова: помимо таймера есть еще и ручное управление, поэтому 
  //выключение происходит не всегда, а только в течении одной минуты, соответствующей таймеру
  if (setMinClockOff == now.minute() && setHorClockOff == now.hour()){
    if ( state != HIGH){
      digitalWrite(outPin, HIGH);
    }
  }
  
  if (setMinClockOn == now.minute() && setHorClockOn == now.hour()){
    if ( state == HIGH){
      digitalWrite(outPin, LOW);
    }
  }


  
  sprintf (s0, "%02i:%02i:%02i %c %02i:%02i", now.hour(), now.minute(), now.second(), state==LOW?'^':'v', setHorClockOn, setMinClockOn);
  lcd.setCursor(0,0);
  lcd.print(s0);

  sprintf (s0, "%02i/%02i/%02i   %02i:%02i", now.day(), now.month(), now.year2(), setHorClockOff, setMinClockOff);
  lcd.setCursor(0,1);
  lcd.print(s0);
  
  delay(200); // нужно для нармальной работы кнопок
}

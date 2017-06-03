#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal.h>

#define DS1307_I2C_ADDRESS 0x68

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

const byte outPin = 13; // выход на реле

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

/////////// часы ..
byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val){
  return ( (val/16*10) + (val%16) );
}

void setDateDs1307(byte second,        // 0-59
                   byte minute,        // 0-59
                   byte hour,          // 1-23
                   byte dayOfWeek,     // 1-7
                   byte dayOfMonth,    // 1-28/29/30/31
                   byte month,         // 1-12
                   byte year)          // 0-99
{
   Wire.beginTransmission(DS1307_I2C_ADDRESS);
   Wire.write(0);
   Wire.write(decToBcd(second));    
   Wire.write(decToBcd(minute));
   Wire.write(decToBcd(hour));     
   Wire.write(decToBcd(dayOfWeek));
   Wire.write(decToBcd(dayOfMonth));
   Wire.write(decToBcd(month));
   Wire.write(decToBcd(year));
   Wire.endTransmission();
}

void getDateDs1307(byte *second,
          byte *minute,
          byte *hour,
          byte *dayOfWeek,
          byte *dayOfMonth,
          byte *month,
          byte *year)
{

  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_I2C_ADDRESS, 7);

  *second     = bcdToDec(Wire.read() & 0x7f);
  *minute     = bcdToDec(Wire.read());
  *hour       = bcdToDec(Wire.read() & 0x3f); 
  *dayOfWeek  = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month      = bcdToDec(Wire.read());
  *year       = bcdToDec(Wire.read());
}
////
void setClock(){ // установка часов
  byte pos = 1;
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year; 
    getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
    lcd.clear();
    lcd.blink();

   while(key() != 1){ // крутим   цикл   
    byte KEY = key(); // читаем состояние кнопок
      delay(200);
    lcd.setCursor(1, 1);
    lcd.print("set to save");
    lcd.setCursor(0, 0);     // выводим инфу
     if (hour < 10) lcd.print("0");
    lcd.print(hour);
    lcd.print(":");
     if (minute < 10) lcd.print("0"); 
    lcd.print(minute);  
    lcd.print(" ");     
     if (dayOfMonth < 10) lcd.print("0");
    lcd.print(dayOfMonth);
    lcd.print("/");
     if (month < 10) lcd.print("0");
    lcd.print(month);
    lcd.print("/");
     if (year < 10) lcd.print("0");
    lcd.print(year);
    
    lcd.setCursor(pos, 0); // устанавливаем курсор согласно позиции
    
    if (KEY == 5 && pos < 13) pos += 3; // крутим позицию
    else if (KEY == 2 && pos > 1) pos -= 3;
    
    else if (pos == 1 && KEY == 3) hour++; // крутим значения
    else if (pos == 1 && KEY == 4) hour--;
    else if (pos == 4 && KEY == 3) minute++;
    else if (pos == 4 && KEY == 4) minute--;    
    else if (pos == 7 && KEY == 3) dayOfMonth++;
    else if (pos == 7 && KEY == 4) dayOfMonth--;    
    else if (pos == 10 && KEY == 3) month++;
    else if (pos == 10 && KEY == 4) month--;    
    else if (pos == 13 && KEY == 3) year++;
    else if (pos == 13 && KEY == 4) year--;  
    
    if (hour > 23) hour = 0;
    else if (minute > 59) minute = 0;
    else if (dayOfMonth > 31) dayOfMonth = 0;
    else if (month > 12) month = 1;
    else if (year > 99) year = 0;
  }// конец цикла
  
 setDateDs1307(second, minute, hour, dayOfWeek, dayOfMonth, month, year); 
   lcd.noBlink(); 
   lcd.clear();
   lcd.print("     Saved");
   delay(1500);
}///

void setOnOff(){    
  byte pos = 0;   
    lcd.clear();
    lcd.blink();

   while(key() != 1){ // крутим   цикл   
    byte KEY = key(); // читаем состояние кнопок
      delay(200);
    lcd.setCursor(1, 1);
    lcd.print("set to save");
    lcd.setCursor(0, 0);     // выводим инфу
     if (setHorClockOn < 10) lcd.print("0");
    lcd.print(setHorClockOn);
    lcd.print(":");
     if (setMinClockOn < 10) lcd.print("0"); 
    lcd.print(setMinClockOn);  
    lcd.print(" ");     
     if (setHorClockOff < 10) lcd.print("0");
    lcd.print(setHorClockOff);
    lcd.print(":");
     if (setMinClockOff < 10) lcd.print("0");
    lcd.print(setMinClockOff); 
    
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
  Wire.begin(); 
  lcd.begin(16, 2);
  lcd.clear();
  
  pinMode(outPin, OUTPUT);
  digitalWrite(outPin, LOW);
  
  setMinClockOn = EEPROM.read(0);
  setHorClockOn = EEPROM.read(1);
  setMinClockOff = EEPROM.read(3);
  setHorClockOff = EEPROM.read(4);
}

void loop()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;  
  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
  // обработка кнопок
  if (key() == 1) menu(); // если нажата селект
  else if (key() == 3) digitalWrite(outPin, HIGH);
  else if (key() == 4) digitalWrite(outPin, LOW);

  // сравниваем время и управляем выходом// 
  if (setMinClockOff == minute && setHorClockOff == hour
      && second == 0) digitalWrite(outPin, LOW);
  if (setMinClockOn == minute && setHorClockOn == hour
      && second == 0) digitalWrite(outPin, HIGH);    
 
 
//   lcd.clear();
    lcd.setCursor(0, 0);
     if (hour < 10) lcd.print("0"); 
    lcd.print(hour); 
    lcd.print(":");
     if (minute < 10) lcd.print("0"); 
    lcd.print(minute);
//  lcd.print(":");
//   if (second < 10) lcd.print("0");
//  lcd.print(second);
//    lcd.setCursor(8, 0); 
//    lcd.print("    ");  
    lcd.setCursor(0, 1);
     if (dayOfMonth < 10) lcd.print("0");
    lcd.print(dayOfMonth);
    lcd.print("/");
     if (month < 10) lcd.print("0");
    lcd.print(month);
    lcd.print("/");
     if (year < 10) lcd.print("0");
    lcd.print(year);
     //
    lcd.setCursor(11, 0);
     if (setHorClockOn < 10) lcd.print("0"); 
    lcd.print(setHorClockOn); 
    lcd.print(":");
     if (setMinClockOn < 10) lcd.print("0"); 
    lcd.print(setMinClockOn);
    
    lcd.setCursor(11, 1);
     if (setHorClockOff < 10) lcd.print("0"); 
    lcd.print(setHorClockOff); 
    lcd.print(":");
     if (setMinClockOff < 10) lcd.print("0"); 
    lcd.print(setMinClockOff);      
   
    lcd.setCursor(7, 0);
    if (digitalRead(outPin)) lcd.print("ON ");
    else lcd.print("Off");
    
    delay(200); // нужно для нармальной работы кнопок
}

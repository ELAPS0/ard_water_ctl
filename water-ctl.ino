#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include "RTClib.h"
RTC_DS1307 RTC;

DateTime now;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
//управление скважным насосом
//Задача: 
//Насос должен автоматически включаться в заданный период (ночью, для экономии электроэнергии) или вручную.
//При включении должен за один цикл непрерывной работы выкачивать не более заданного количества литров. После достижения лимита - пауза и новый цикл.
//Количество циклов и длинна пауз - настраиваемые.
//Вода может поступать на два получателя - бочки и канава (для прокачки скважены). Переключение между получателями осуществляется электромакнитным клапаном.
//При запуске клапан закрыт и вода поступает в бочки. Если во время подачи питания на насос поток не идет, то включаем клапан и льём в канаву.

// выход на реле насоса
//подача HIGH на реле выключает (!) нагрузку
//LOW - включает
const byte pumpCtlPin = 2; 

// выход на реле электромагнитного клапана
//подача HIGH на реле выключает (!) нагрузку
//LOW - включает
const byte valveCtlPin = 7; 

//датчик потока
const byte flowPin = 3;

//количество жидкости выкачиваемое за одно включение (л)
const uint32_t once_vol_limit = 400;
//количество жидкости выкачиваемое за период работы таймера (л)
const uint32_t total_vol_limit = 2000;
//количество выкаченной жидкости за одно включение
volatile uint32_t  current_vol = 0;
//количество выкаченной жидкости за период
volatile uint32_t total_vol = 0;

//Время последнего изменения состояния (в секундах от 1.01.2000 )
uint32_t change_state_time = 0;
//временной промежуток, через который открывается клапан в том случае, если поток не привышает значение drift_zero (в секундах)
uint32_t flow_wait_time = 10;
//значение счетчика потока, которое считается не отличимо от нуля
uint32_t drift_zero = 10;

//состояние устройства  
enum State {
      UPLOAD,   //качает воду
      PAUSED,   //приостановлено для набора воды    
      STOPPED   //отдыхает
    } state;


byte setMinClockOn; // 
byte setHorClockOn;
byte setMinClockOff; // 
byte setHorClockOff;


//обработчик прерывания от датчика потока
void flowChange(){
 current_vol++;
 total_vol++;
}

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


//работа с насосом
void pump_stop()
{
  digitalWrite(pumpCtlPin, HIGH);
  digitalWrite(valveCtlPin, HIGH);

  if (STOPPED != state)
  {
      Serial.println("Switch to the stopped state");
      change_state_time = now.secondstime();
  }
  
  current_vol = total_vol = 0;
  state = STOPPED;
}

void pump_pause()
{
  digitalWrite(pumpCtlPin, HIGH);
  digitalWrite(valveCtlPin, HIGH);

  if (PAUSED != state)
  {
      Serial.println("Switch to the paused state");
      change_state_time = now.secondstime();
  }
  current_vol = 0;   
  state = PAUSED;
}

void pump_upload()
{
  digitalWrite(pumpCtlPin, LOW);


  if (UPLOAD != state)
  {
      Serial.println("Switch to the upload state");
      change_state_time = now.secondstime();
  }
  
  state = UPLOAD;
}


void setup(){
  //отладка 
  Serial.begin(9600);
  
  RTC.begin();
  
  Wire.begin(); 
  lcd.begin(16, 2);
  lcd.clear();
  
  pinMode(pumpCtlPin, OUTPUT);
  pinMode(valveCtlPin, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(flowPin), flowChange, RISING);
  
  setMinClockOn = EEPROM.read(0);
  setHorClockOn = EEPROM.read(1);
  setMinClockOff = EEPROM.read(3);
  setHorClockOff = EEPROM.read(4);
  
  if (! RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }

  now = RTC.now();

  pump_stop();
}

void loop()
{
  
  char s0[16];
 
    
  // обработка кнопок
  if (key() == 1) menu(); // если нажата селект
  else if (key() == 3) pump_upload();
  else if (key() == 4) pump_stop();


  // сравниваем время и управляем выходом// 
  //логика такова: помимо таймера есть еще и ручное управление, поэтому 
  //выключение происходит не всегда, а только в течении одной минуты, соответствующей таймеру
  if (setMinClockOff == now.minute() && setHorClockOff == now.hour()){
    pump_stop();
  }
  
  if (setMinClockOn == now.minute() && setHorClockOn == now.hour()){
    pump_upload();
  }

  switch (state){
    case UPLOAD:
      //устройство включено
      if (total_vol < total_vol_limit){
        //выкачали меньше чем нужно, продолжаем
        if (current_vol < once_vol_limit){
          //выкачали меньше, чем разрешено за один подход
          //проверим, качаем ли мы вообще
          if (flow < drift_zero && now.secondstime() - change_state_time > flow_wait_time){
            Serial.println("open valve");
            digitalWrite(valveCtlPin, LOW);
          }
            
        }else{
          pump_pause();
        }
      }else{
        //выкачали сколько нужно, выключаемся
        Serial.println("Rise flow limit"); 
        pump_stop();       
      }
      if ( state == HIGH){
        digitalWrite(pumpCtlPin, LOW);
       break;
    case PAUSED:
      if (now.secondstime() - pause_start > pause_len){
        if (setMinClockOff == now.minute() && setHorClockOff == now.hour()){
          pump_stop();
        }else{
          pump_upload();
        }
      }
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

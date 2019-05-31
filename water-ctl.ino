#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <RTClib.h>
#include "Digipin.h"
#include "septik_rx.h"
#include <bob_aux.h>


#define _BOB_DEBUG
RTC_DS1307 RTC;

DateTime now;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
//управление скважным насосом и контроль переполнения септика
//про насос: 
//Насос должен автоматически включаться в заданный период (ночью, для экономии электроэнергии) или вручную.
//При включении должен за один цикл непрерывной работы выкачивать не более заданного количества литров define . После достижения лимита - пауза и новый цикл.
//Количество циклов и длинна пауз - настраиваемые.
//Вода может поступать на два получателя - бочки и канава (для прокачки скважены). Переключение между получателями осуществляется электромакнитным клапаном.
//При запуске клапан закрыт и вода поступает в бочки. Если во время подачи питания на насос поток не идет, то включаем клапан и льём в канаву.
//
//Про септик:
//в нормальном состоянии с септика приходят пакеты проверки канала. в случае их отсутствия необходимо выключить питание на септике, отключить насос, поддерживающий давление в системе,
//запретить использовать скважный насос 
//электромагнитный клапан греется, когда открыт, поэтому включать его нужно тогда, когда через него идет вода.


#define DEEP_PUMP_PIN       2  //выход управления реле скважного насоса
#define FLOW_SENSOR_PIN     3  //вход датчика поока
#define RX_PIN              10 //вход приемника радиоканала от септика
#define SEPTIK_POWER_PIN    11 //выход управления реле питания септика
#define PREASURE_PUMP_PIN   12 //выход управления реле повышающего насоса
#define VALVE_PIN           13  //выход управления электромагнитным клапаном для прокачки через септик

#define FLOW_SENSOR_ENABLE  0 //используется датчик потока
#define LEVEL_SENSOR_ENABLE 0 //используются датчики уровня 

#define RX_FAIL_LIMIT   10
SeptikRX septik(RX_PIN, RX_FAIL_LIMIT);

// выход на реле скважного насоса
//подача HIGH на реле выключает (!) нагрузку
//LOW - включает

//подключены на нормально разомкнутые реле, по умолчанию - питания нет
Digipin deep_pump; 
Digipin valve;

//подключены на нормально замкнутые реле, по умолчанию - питание подано
Digipin septik_power;
Digipin preasure_pump;


//количество жидкости выкачиваемое за одно включение (л)
const uint32_t once_vol_limit = 400;
//количество жидкости выкачиваемое за период работы таймера (л)
const uint32_t total_vol_limit = 1000;
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


//длина паузы в секундах (ждем пока вода наберется)
uint32_t pause_len = 3600;
//время начала паузы для пополнения скважены
uint32_t pause_start_time;

/*
 * состояние контроллера
 */
typedef void (*Way)();

enum CTL_STATE {
    CTL_IDLE,               //Все остановлено
    CTL_TANK_UPLOAD,        //Набираются бочки
    CTL_TANK_UPLOAD_PAUSE,  //пауза для того, чтобы не опусташать скважену полностью
    CTL_UPLOAD_OUT,          //бочки полные, сливаем воду в канаву
    CTL_SIZE
};


CTL_STATE current = CTL_IDLE;
CTL_STATE new_state = CTL_IDLE;

Way ctl_ways [CTL_SIZE][CTL_SIZE]; 


void ctl_idle()
{
  BOB_TRACE("Idle");
  deep_pump_stop();
}

void idle2tank_upload()
{
  BOB_TRACE("idle2tank_upload");
  deep_pump_start();
}

void tank_upload2upload_pause()
{
  BOB_TRACE("tank_upload2upload_pause");
  deep_pump_stop();
  pause_start_time = now.unixtime();
}

void tank_upload2upload_out()
{
  BOB_TRACE("tank_upload2upload_out");
  deep_pump_start();
  valve.turn_on();
  
}

void tank_upload2idle()
{
  BOB_TRACE("tank_upload2idle");
  deep_pump_stop();
}

void upload_pause2idle()
{
  BOB_TRACE("upload_pause2idle");
}

void upload_pause2tank_upload()
{
  BOB_TRACE("upload_pause2tank_upload");
  deep_pump_start();
  valve.turn_off();
}

void upload_out2idle()
{
  BOB_TRACE("upload_out2idle");
  deep_pump_stop();
}

void change_state()
{
  if (ctl_ways[current][new_state])
  {
    ctl_ways[current][new_state]();
    current = new_state;
  }
}

void ctl_init()
{
  memset(ctl_ways, sizeof(ctl_ways), sizeof(Way));
  ctl_ways[CTL_IDLE][CTL_IDLE] = &ctl_idle;
  ctl_ways[CTL_IDLE][CTL_TANK_UPLOAD] = &idle2tank_upload;
  ctl_ways[CTL_TANK_UPLOAD][CTL_IDLE] = &tank_upload2idle;
  ctl_ways[CTL_TANK_UPLOAD][CTL_TANK_UPLOAD_PAUSE] = &tank_upload2upload_pause;
  ctl_ways[CTL_TANK_UPLOAD][CTL_UPLOAD_OUT] = &tank_upload2upload_out;
  ctl_ways[CTL_TANK_UPLOAD_PAUSE][CTL_TANK_UPLOAD] = &upload_pause2tank_upload;
  ctl_ways[CTL_TANK_UPLOAD_PAUSE][CTL_IDLE] = &upload_pause2idle;
  ctl_ways[CTL_UPLOAD_OUT][CTL_IDLE] = &upload_out2idle;
}

//----------------------
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


//работа со скважным насосом
void deep_pump_stop()
{
  if (deep_pump.get_state() != DIGIPIN_OFF)
  {
      Serial.println("Deep pump stopped");
      change_state_time = now.unixtime();
  }
  deep_pump.turn_off();
  valve.turn_off();
  current_vol = total_vol = 0;
}

void deep_pump_start()
{
  if (deep_pump.get_state() != DIGIPIN_ON)
  {
      Serial.println("Deep pump started");
      change_state_time = now.unixtime();
  } 
  deep_pump.turn_on();
  current_vol = total_vol = 0;
}


void setup(){
  //отладка 
  Serial.begin(9600);
  
  RTC.begin();
  
  Wire.begin(); 
  lcd.begin(16, 2);
  lcd.clear();
  
  deep_pump.init(DEEP_PUMP_PIN, "Deep pump", DIGIPIN_OFF, true); 
  valve.init(VALVE_PIN, "Valve", DIGIPIN_OFF, true); 
  septik_power.init(SEPTIK_POWER_PIN, "Septik power", DIGIPIN_ON, false); 
  preasure_pump.init(PREASURE_PUMP_PIN, "Preasure pump", DIGIPIN_ON, false); 

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowChange, RISING);
  
  setMinClockOn = EEPROM.read(0);
  setHorClockOn = EEPROM.read(1);
  setMinClockOff = EEPROM.read(3);
  setHorClockOff = EEPROM.read(4);
  
  if (! RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }

  now = RTC.now();

  //septik.init(RF433_RATE);
  ctl_init();
  Serial.println("setup done");
}

void loop()
{
  
  char s0[16];
  now = RTC.now();
    
  // обработка кнопок
  if (key() == 1) menu(); // если нажата селект
  else if (key() == 3 && (CTL_TANK_UPLOAD_PAUSE != current || CTL_UPLOAD_OUT != current)) new_state = CTL_TANK_UPLOAD; //пока так, когда будет отдельное управление клапаном - переделаю
  else if (key() == 4) new_state = CTL_IDLE;

  // сравниваем время и управляем выходом// 
  //логика такова: помимо таймера есть еще и ручное управление, поэтому 
  //выключение происходит не всегда, а только в течении одной минуты, соответствующей таймеру
  if (setMinClockOff == now.minute() && setHorClockOff == now.hour()){
    new_state = CTL_IDLE;
  }
  
  if (setMinClockOn == now.minute() && setHorClockOn == now.hour() && (CTL_TANK_UPLOAD_PAUSE != current || CTL_UPLOAD_OUT != current)){
    new_state = CTL_TANK_UPLOAD;
  }


  switch (current){
    
    case CTL_TANK_UPLOAD:
      //обработать показания верхнего датчика уровня NYI.  
      //обработка показаний датчика потока
      if (current_vol > once_vol_limit){
        // выкачили сколько нужно, переходим в состояние паузы, чтобы набралась скважена 
        new_state = CTL_TANK_UPLOAD_PAUSE;
      }
      else{
        //а может вообще не качаем?
        //пока не подключен датчик - не используем
        if (0 && current_vol < drift_zero && now.unixtime() - change_state_time > flow_wait_time){
          //либо все сломалось, либо бочки полные. Сливаем в канаву
          new_state = CTL_UPLOAD_OUT;
        }
      }
      break;

    case CTL_TANK_UPLOAD_PAUSE:
      if ( now.unixtime() - pause_start_time > pause_len)
        new_state = CTL_TANK_UPLOAD;
      break;
  }
    
  change_state();

/*
  if (septik.check()){
    if (!septik.is_state_normal()){
      //все выключаем и ждем вмешательства оператора
      Serial.println("Septik fail. Power off");
      septik_power.turn_off();
      preasure_pump.turn_off();
      valve.turn_off();
      deep_pump.turn_off();
      while(1){
        lcd.setCursor(0,0);
        lcd.print("Septik failed. Power off");
        sprintf (s0, "%02i/%02i/%02i   %02i:%02i", now.day(), now.month(), now.year2(), setHorClockOff, setMinClockOff);
        lcd.setCursor(0,1);
        lcd.print(s0);
        delay(300);
      }
    }
  } 
*/


    
  sprintf (s0, "%02i:%02i:%02i %c %02i:%02i", now.hour(), now.minute(), now.second(), CTL_TANK_UPLOAD==current || CTL_UPLOAD_OUT==current?'^':'v', setHorClockOn, setMinClockOn);
  lcd.setCursor(0,0);
  lcd.print(s0);

  sprintf (s0, "%02i/%02i/%02i   %02i:%02i", now.day(), now.month(), now.year2(), setHorClockOff, setMinClockOff);
  lcd.setCursor(0,1);
  lcd.print(s0);
  
  delay(200); // нужно для нармальной работы кнопок
}

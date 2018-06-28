 //Контроллер вентилятора в ванной комнате. Версия 3.0 Mysensors
// Enable debug prints
#define MY_DEBUG
#define DEBUG

// Параметры NRF24L01
// Enable passive mode
//#define MY_PASSIVE_NODE
#define MY_TRANSPORT_UPLINK_CHECK_DISABLED
#define PIN_NRF_POWER     8
#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC
#define MY_NODE_ID        30
#define MY_RF24_CE_PIN    9
#define MY_RF24_CS_PIN    10
//#define MY_RF24_POWER_PIN (PIN_NRF_POWER)
#define RF24_CHANNEL      76
//#define MY_RF24_DATARATE (RF24_1MBPS)

// Определение пинов контроллера
#define PIN_RELAY        16
#define PIN_DHT          15
#define PIN_CLK          19
#define PIN_DIO          18
#define PIN_ANALOG       A3
#define PIN_BUTTON        2
#define TZ                5
#define ERROR_VALUE 2147483647

#define MY_RADIO_NRF24
//#define MY_RF24_PA_LEVEL (RF24_PA_MIN)

//#include <avr/wdt.h>
#include <MySensors.h>  
#include <TM1637.h>
#include <DHT.h> 
#include "WC_button.h"

// Глобальные переменные контроллера
int Temp  = 0;
int Hum   = 0;
uint16_t ALight = 0;
bool is_fan     = false;
bool is_manual  = false;
bool is_light   = false;
bool is_hum     = false;
bool is_change  = true;

struct CONFIG {
  uint16_t HLEVEL;
  uint16_t ALEVEL;
  uint32_t TIMER;
  uint16_t  SRC;
}Config;

TM1637 tm1637(PIN_CLK,PIN_DIO);
DHT dht(PIN_DHT, AM2301); 
SButton b1(PIN_BUTTON ,200,0,0,0,0); 

// Define sensor node childs
#define CHILD_ID_TEMP     0
#define CHILD_ID_HUM      1
#define CHILD_ID_LIGHT    2
#define CHILD_ID_FAN      3
#define CHILD_ID_MANUAL   4
#define CHILD_ID_CONTROL  10
#define CHILD_ID_HLEVEL   100
#define CHILD_ID_ALEVEL   101
#define CHILD_ID_TIMER    102

bool is_time = false;
uint32_t t_cur      = 0;    
long  t_correct     = 0;
uint32_t tm         = 0;
uint32_t Timer      = 0;
bool points         = false;

uint32_t ms, ms0=0, ms1=0, ms2=0, ms3=0, ms4=0;
uint8_t disp_mode   = 0;

MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgLight(CHILD_ID_LIGHT, V_STATUS);
MyMessage msgFan(CHILD_ID_FAN, V_STATUS);
MyMessage msgManual(CHILD_ID_MANUAL, V_STATUS);
MyMessage msgHLevel(CHILD_ID_HLEVEL, V_CUSTOM);
MyMessage msgALevel(CHILD_ID_ALEVEL, V_CUSTOM);
MyMessage msgTimer(CHILD_ID_TIMER, V_CUSTOM);
MyMessage msgControl(CHILD_ID_CONTROL, V_STATUS);


float readVcc();
void(* resetFunc) (void) = 0; // Reset MC function 

void before(){
  pinMode(PIN_RELAY,OUTPUT);
  digitalWrite(PIN_RELAY,LOW);
  pinMode(PIN_NRF_POWER,OUTPUT);
  digitalWrite(PIN_NRF_POWER,HIGH);
  delay(200);
// Бываеи иногда нужна очистка памяти NRF24  
//  for (int i=0; i<EEPROM_LOCAL_CONFIG_ADDRESS; i++)      hwWriteConfig(i,0xFF);
//  _ReInit();
// Инициализация дисплея
   Serial.println("Display TM1637 init\n");
   tm1637.init();
   tm1637.set(7);
   tm1637.display(0,0);
   tm1637.display(1,0);
   tm1637.display(2,0);
   tm1637.display(3,0);
  
}

void setup()
{
#ifdef DEBUG  
   Serial.begin(115200);
   Serial.println("MySensor Fan Controller");
#endif   
// Инициализация DHT21
   Serial.print("Init DHT21 on ... ");
   dht.begin(); 
   delay(500);
   long ret = (long)dht.readTemperature();
       Serial.println(ret);
   }
   else {
      Serial.println("fail");
   }
   b1.begin();
   CONFIG_read();
   SendConfig();
}

void presentation()
{
	sendSketchInfo("FAN", "3.0");

  present(CHILD_ID_TEMP, S_TEMP,"C");
	present(CHILD_ID_HUM, S_HUM,"%");
  present(CHILD_ID_LIGHT, S_BINARY);
  present(CHILD_ID_FAN, S_BINARY);
  present(CHILD_ID_MANUAL, S_BINARY);
  present(CHILD_ID_HLEVEL, S_CUSTOM);
  present(CHILD_ID_ALEVEL, S_CUSTOM);
  present(CHILD_ID_TIMER, S_CUSTOM);
  present(CHILD_ID_CONTROL, S_BINARY);
}

void loop(){
   ms  = millis();
   t_cur       = ms/1000;

// Нажата кнопка
   if( b1.Loop() == SB_CLICK ){
#ifdef DEBUG
   Serial.println("Button click");
#endif      
      if( is_manual == false ){
         is_manual = true;
         is_change = true;        
         is_fan    = !is_fan;
         digitalWrite(PIN_RELAY,is_fan);
         Timer     = ms; 
      }
      else {
         is_manual = false;
         is_change = true;
         is_fan    = false;
         digitalWrite(PIN_RELAY,is_fan);
         Timer     = 0;         
      }
   }
  
  if( ( ms0 == 0 || ms0 > ms || ms - ms0 > 10000 )&&is_time == false ){
    ms0 = ms;
    requestTime();  
  }
  if( ( ms1 == 0 || ms1 > ms || ms - ms1 > 600000 ) ){
    ms1 = ms;
    is_time = false;      
  }

// Событие каждые 0.5 сек  
  if(ms2 == 0 || ms < ms2 || (ms - ms2) > 500 ){
      ms2 = ms;
      Temp    = (int)dht.readTemperature();
      Hum     = (int)dht.readHumidity();
      ALight  = analogRead(PIN_ANALOG);  
// Влажность выше нормы       
      if( Hum > Config.HLEVEL+5 && is_hum == false ){
         is_hum    = true;
         is_change = true;
      }
// Влажность снова в норме      
      if( Hum < Config.HLEVEL && is_hum == true ){
         is_hum    = false;
         is_change = true;
      }
// Фиксация включения света      
      if( ALight < Config.ALEVEL && is_light == false ){
         is_light  = true;
         is_change = true;
      }
// Фиксация выключения света      
      if( ALight >= Config.ALEVEL && is_light == true ){
         is_light  = false;
         is_change = true;
      }
      if( is_change )MakeController();
      tm    = t_cur + t_correct;
      DisplayShow(disp_mode);
  }

  if( ((ms3 == 0 || ms < ms3 || (ms - ms3) > 15000)&&is_hum == true)||
     ((ms3 == 0 || ms < ms3 || (ms - ms3) > 300000)&&is_hum == false) ){
      ms3 = ms;
      send(msgHum.set(Hum));
      send(msgTemp.set(Temp));
#ifdef DEBUG
      Serial.print("t=");
      Serial.print(Temp);
      Serial.print(" h=");
      Serial.print(Hum);
      Serial.print(" a=");
      Serial.println(ALight);
#endif      
  }
  if( ms4 == 0 || ms < ms4 || (ms - ms4) > 10000 ){
      ms4 = ms;
      if( disp_mode < 2 )disp_mode++;
      else disp_mode = 0;
  }
// Таймер закончился  
  if( is_manual == true && ms - Timer > Config.TIMER ){
     is_manual = false;
     is_change = true;
     is_fan    = false;
     digitalWrite(PIN_RELAY, is_fan);
     Timer     = 0;         
  }

  wait( 300 );
}


/**
 * Считываем напряжение питания
 */
float readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = (1100L * 1023)/result;
  float ret = ((float)result)/1000;
  return ret;
}

// This is called when a new time value was received

void receiveTime(uint32_t controllerTime)
{
  uint32_t t = controllerTime+3600*TZ;
  int   m = ( t/60 )%60;
  int   h = ( t/3600 )%24;
  char s[10];
  sprintf(s,"%02d:%02d",h,m);
  Serial.print("Time value received: ");
  Serial.println(s);
  t_correct = t - t_cur;

  is_time = true;
} 


void receive(const MyMessage &message){
  uint16_t x;
  uint32_t y;
  switch( message.sensor ){
     case CHILD_ID_HLEVEL :
        x = atoi(message.data);
        if( x>0 && x < 100)Config.HLEVEL = x;
        CONFIG_save();
        SendConfig();
        break;
     case CHILD_ID_ALEVEL :
        x = atoi(message.data);
        if( x>0 && x < 1024)Config.ALEVEL = x;
        CONFIG_save();
        SendConfig();
        break;
     case CHILD_ID_TIMER :
        y = atol(message.data);
        if( x>10000 && x < 3600000)Config.TIMER = y;
        CONFIG_save();
        SendConfig();
        break;
     case CHILD_ID_CONTROL :
        if( is_manual == false ){
           is_manual = true;
           is_change = true;        
           is_fan    = !is_fan;
           digitalWrite(PIN_RELAY,is_fan);
           Timer     = ms; 
        }
        else {
           is_manual = false;
           is_change = true;
           is_fan    = false;
           digitalWrite(PIN_RELAY,is_fan);
           Timer     = 0;         
        }
        break;
        
  }
  
  Serial.print("Type=");
  Serial.print(message.type);
  Serial.print(" Sender=");
  Serial.print(message.sender);
  Serial.print(" Dest=");
  Serial.print(message.destination);
  Serial.print(" Data=");
  Serial.print(message.data);
  Serial.print(" Sensor=");
  Serial.print(message.sensor);
  Serial.println("");  

}


void DisplayShow(uint8_t mode){
   int m = 0, h = 0;
   if( is_manual == true ){
      uint32_t timer = (Config.TIMER - (ms - Timer))/1000;
      m = (timer)%60;
      h = timer/60;
      tm1637.clearDisplay();
      tm1637.point(points);
      tm1637.display(0,h/10);
      tm1637.display(1,h%10);
      tm1637.display(2,m/10);
      tm1637.display(3,m%10);                  
      points = !points;
   }
   else {
       if( mode == 0 ){
// Отображение времени        
          m = ( tm/60 )%60;
          h = ( tm/3600 )%24;
          tm1637.clearDisplay();
          tm1637.point(points);
          tm1637.display(0,h/10);
          tm1637.display(1,h%10);
          tm1637.display(2,m/10);
          tm1637.display(3,m%10);       
          points = !points;
       }
       else if( mode == 1 ){
           tm1637.clearDisplay();
           tm1637.point(false);
           tm1637.display(0,Hum/10);
           tm1637.display(1,Hum%10);
           DisplaySpecialChar(3,0x24);        
       }
       else {
           tm1637.clearDisplay();
           tm1637.point(false);
           tm1637.display(0,Temp/10);
           tm1637.display(1,Temp%10);
           DisplaySpecialChar(2,0x63);
           tm1637.display(3,0x0C);        
       }
   }        
}

/**
 * Вывести спец-символ на дисплей
 */
void DisplaySpecialChar(uint8_t BitAddr,int8_t SpecChar)
{
  tm1637.start();          //start signal sent to TM1637 from MCU
  tm1637.writeByte(ADDR_FIXED);//

  tm1637.stop();           //
  tm1637.start();          //
  tm1637.writeByte(BitAddr|0xc0);//
  
  tm1637.writeByte(SpecChar);//
  tm1637.stop();            //
  tm1637.start();          //
  
  tm1637.writeByte(tm1637.Cmd_DispCtrl);//
  tm1637.stop();           //
}

/**
 * Послать конфигурационные параметры серверу
 */
void SendConfig(){
   send(msgHLevel.set(Config.HLEVEL));
   send(msgALevel.set(Config.ALEVEL));
   send(msgTimer.set(Config.TIMER));
#ifdef DEBUG
   Serial.print("HLEVEL=");
   Serial.print(Config.HLEVEL);
   Serial.print(" ALEVEL=");
   Serial.print(Config.ALEVEL);
   Serial.print(" TIMER=");
   Serial.println(Config.TIMER);
#endif   
}

/**
 * Изменение состояния контроллера
 */
void MakeController(){
// Проверка что ручной режим выключен  
   if( is_manual == false ){
// Если свет выключен и влажность высокая включить вентилятор
      if( is_light == false && is_hum == true && is_fan == false ){
         is_fan = true;
         digitalWrite( PIN_RELAY,is_fan);         
      }
// Если свет включен и влажность высокая то выключить вентилятор
      if( is_light == true && is_hum == true && is_fan == true ){
         is_fan = false;
         digitalWrite( PIN_RELAY,is_fan);         
      }
// Если влажность низкая то выключить вентилятор
      if( is_hum == false && is_fan == true ){
         is_fan = false;
         digitalWrite( PIN_RELAY,is_fan);                
      }
   }
   send(msgLight.set((int)is_light));
   send(msgFan.set((int)is_fan));
   send(msgManual.set((int)is_manual));
#ifdef DEBUG
   Serial.print("hum=");
   Serial.print(Hum);
   Serial.print(" is_light=");
   Serial.print(is_light);
   Serial.print(" is_hum=");
   Serial.print(is_hum);
   Serial.print(" is_fan=");
   Serial.print(is_fan);
   Serial.print(" is_manual=");
   Serial.println(is_manual);
#endif
   
   is_change = false;
   
}

/**
 * Читаем конфигурацию из EEPROM в память
 */
void CONFIG_read(void){
   uint8_t sz1 = (uint8_t)sizeof(Config);
   for( uint8_t i=0; i<sz1; i++ ){
       uint8_t c = loadState(i);
       *((uint8_t*)&Config + i) = c; 
    }  
    uint16_t src = CONFIG_SRC();
    if( Config.SRC == src ){
#ifdef DEBUG      
       Serial.println("EEPROM Config is correct\n");
#endif      
    }
    else {
#ifdef DEBUG      
       Serial.println("EEPROM SRC is not valid");
#endif      
       Config.HLEVEL = 60;
       Config.ALEVEL = 300;
       Config.TIMER   = 300000;
       CONFIG_save();
    }        
}


/**
 * Сохраняем значение конфигурации в EEPROM
 */
void CONFIG_save(void){
   Config.SRC = CONFIG_SRC();
   uint8_t sz1 = (uint8_t)sizeof(Config);
   for( uint8_t i=0; i<sz1; i++)
      saveState(i, *((uint8_t*)&Config + i));
#ifdef DEBUG      
   Serial.println("Save Config to EEPROM");   
#endif   
}

/**
 * Сохраняем значение конфигурации в EEPROM
 */
uint16_t CONFIG_SRC(void){
   uint16_t src = 0;
   uint8_t sz1 = (uint8_t)sizeof(Config);
   uint16_t src_save = Config.SRC;
   Config.SRC = 0;
   for( uint8_t i=0; i<sz1; i++)src +=*((uint8_t*)&Config + i);
#ifdef DEBUG      
   Serial.print("SCR="); 
   Serial.println(src);
#endif   
   Config.SRC = src_save;
   return src;  
}



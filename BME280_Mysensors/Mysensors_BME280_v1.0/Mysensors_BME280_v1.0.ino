#include <Wire.h>
#include <SPI.h>
#include <SparkFunBME280.>"

//Датчик температуры с батарейным питанием
// Enable debug prints
//#define MY_DEBUG
//#define DEBUG

// Enable passive mode
#define MY_PASSIVE_NODE

// Passive mode requires static node ID
#define MY_NODE_ID 60
#define MY_RF24_CE_PIN 9
#define MY_RF24_CS_PIN 10
#define MY_RF24_POWER_PIN 8
#define RF24_CHANNEL     76

#define MY_RADIO_NRF24

#include <MySensors.h>  

BME280 bme;
// Define sensor node childs
#define CHILD_ID_VCC   0
#define CHILD_ID_TEMP  1
#define CHILD_ID_HUM   2
#define CHILD_ID_PRESS 3

MyMessage msgVcc(CHILD_ID_VCC, V_VOLTAGE);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgPress(CHILD_ID_PRESS, V_PRESSURE);

// Время сна между посылками пакетов
#define TM_SLEEP 300000
// Время между перезагрузками (0 если не нужно)
#define TM_RESET 43200000


float readVcc();
void _ReInit();
void(* resetFunc) (void) = 0; // Reset MC function 

void before(){
//  Зажигаем светодиод
  pinMode(15,OUTPUT);
  digitalWrite(15,HIGH);
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Temp/num/press sensor", "1.0");
  present(CHILD_ID_TEMP, S_TEMP,"Temperature,C");
  present(CHILD_ID_HUM,  S_HUM,"Humidity, %");
  present(CHILD_ID_PRESS, S_BARO,"Pressure");
}  

void setup(){
#ifdef DEBUG  
  Serial.begin(115200);
  Serial.println("Reading basic values from BME280");
#endif  
// Инициализируем BME280
  Wire.begin();
  bme.setI2CAddress(0x76); //Connect to a second sensor
  if (bme.beginI2C() == false) {
#ifdef DEBUG  
    Serial.println("The sensor did not respond. Please check wiring.");
#endif    
  }
// Гасим светодиод и уходим в основной цикл  
   digitalWrite(15,LOW);
// BME280 переводим в режим низкого жнергопотребления   
   bme.setMode(MODE_SLEEP); //Sleep for now 
}

void loop(){
  digitalWrite(15,HIGH);
  float vcc = readVcc();
  bme.setMode(MODE_FORCED); //Sleep for now 
  float t   = bme.readTempC();
  float h   = bme.readFloatHumidity();
  float p   = bme.readFloatPressure()/133;
  float a   = bme.readFloatAltitudeMeters();
  bme.setMode(MODE_SLEEP); //Sleep for now 

#ifdef DEBUG   
  Serial.print("Humidity: ");
  Serial.print(h, 0);
  Serial.print(" Pressure: ");
  Serial.print(p, 0);
  Serial.print(" Alt: ");
  Serial.print(a, 1);
  Serial.print(" Temp: ");
  Serial.print(t, 2);
  Serial.print(" Vcc: ");
  Serial.print(vcc);
  Serial.println();
#endif
  
  _ReInit();
  send(msgVcc.set(vcc,2));
  send(msgTemp.set(t,1));
  send(msgHum.set(h,0));
  send(msgPress.set(p,0));

  digitalWrite(15,LOW);
  sleep( TM_SLEEP );
  if( TM_RESET != 0L && millis() > TM_RESET ){
     resetFunc();  
  }
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

/**
 * Реинициализация NRF-ки (выдрано из библиотеки)
 * Без него не выходит из режима низкого энергопотребления
 */
void _ReInit(){
    // Save static parent ID in eeprom (used by bootloader)
  hwWriteConfig(EEPROM_PARENT_NODE_ID_ADDRESS, MY_PARENT_NODE_ID);
  // Initialise transport layer
  transportInitialise();
  // Register transport=ready callback
  transportRegisterReadyCallback(_callbackTransportReady);
  // wait until transport is ready
  (void)transportWaitUntilReady(MY_TRANSPORT_WAIT_READY_MS);
}

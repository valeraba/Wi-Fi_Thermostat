#define COUNT_SIGNALS 6
#define COUNT_STORE 3
#include "Types.h"
#include "MgtClient.h"

#include "ESP8266_Board.h"
#include <ESP8266WiFi.h>

#include "WC_EEPROM.h"
#include "WC_HTTP.h"
#include "sav_button.h"
#include <OneWire.h>
#include <Ticker.h>

#define PIN_BUTTON   0
#define PIN_RELAY    12
#define PIN_LED_MODE 13
#define PIN_ONEWIRE_1  14
#define PIN_ONEWIRE_2  4


const char* WIFI_SSID = EC_Config.ssid;
const char* WIFI_PASSWORD = EC_Config.pass;

extern struct PortableSocket mySocket;

Ticker ticker;

bool relay_on = false;
uint8_t blink_mode = 0B00000101;

SButton b1(PIN_BUTTON , 100, 5000, 0, 0, 0);

void debugLog(const __FlashStringHelper* aFormat, ...) {
  va_list args;
  va_start(args, aFormat);
  char buf[100];
  vsnprintf_P(buf, sizeof(buf), (const char*)aFormat, args);
  Serial.print(buf);
  va_end(args);
}

static struct Signal* s1; // relay
static struct Signal* s2; // temperature
static struct Signal* s3; // temperature-2
static struct Signal* s4; // min_temperature
static struct Signal* s5; // max_temperature
static struct Signal* s6; // isAutoMode

static struct MgtClient client;

// write with confirmation for "relay"
static void write_s1(bool aValue) {
  relay_on = aValue;
  digitalWrite(PIN_RELAY, aValue);

  signal_update_int(s1, aValue, getUTCTime());
  mgt_writeAns(&client, s1, erOk); // confirmation
  debugLog(F("write s1: %i\n"), aValue);
}

// write with confirmation for "min_temperature"
static void write_s4(float aValue) {
  EC_Config.minTemperature = aValue;
  EC_save();
  signal_update_double(s4, aValue, getUTCTime());
  mgt_writeAns(&client, s4, erOk); // confirmation

}

// write with confirmation for "max_temperature"
static void write_s5(float aValue) {
  EC_Config.maxTemperature = aValue;
  EC_save();
  signal_update_double(s5, aValue, getUTCTime());
  mgt_writeAns(&client, s5, erOk); // confirmation

}

// write with confirmation for "isAutoMode"
static void write_s6(bool aValue) {
  EC_Config.isAutoMode = aValue;
  EC_save();
  signal_update_int(s6, aValue, getUTCTime());
  mgt_writeAns(&client, s6, erOk); // confirmation
}

static void handler(enum OpCode aOpCode, struct Signal* aSignal, struct SignalValue* aWriteValue) {
  switch (aOpCode) {
    case opRead:
      signal_updateTime(aSignal, getUTCTime());
      mgt_readAns(&client, aSignal, erOk);
      break;
    case opWrite:
      if (aSignal == s1)
        write_s1(aWriteValue->u.m_bool);
      else if (aSignal == s4)
        write_s4(aWriteValue->u.m_float);
      else if (aSignal == s5)
        write_s5(aWriteValue->u.m_float);
      else if (aSignal == s6)
        write_s6(aWriteValue->u.m_bool);
      break;
    case opWriteAsync:
      break;
  }
}

// Событие срабатывающее каждые 125 мс
void tick() {
  static uint8_t blink_loop = 0;
  // Режим светодиода ищем по битовой маске
  if (blink_mode & 1 << (blink_loop & 0x07))
    digitalWrite(PIN_LED_MODE, LOW);
  else
    digitalWrite(PIN_LED_MODE, HIGH);
  blink_loop++;
}

void setup() {
  // Последовательный порт для отладки
  Serial.begin(115200);
  debugLog(F("\n\nFree memory %d\n"), ESP.getFreeHeap());

  // Инициализация EEPROM
  EC_begin();
  EC_read();

  // Подключаемся к WiFi
  WiFi_begin();
  if (WiFi.status() == WL_CONNECTED) blink_mode = 0B11111111;
  else blink_mode = 0B00000101;
  delay(2000);

  // Старт внутреннего WEB-сервера
  HTTP_begin();

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED_MODE, OUTPUT);
  b1.begin();

  delay(2000);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED_MODE, OUTPUT);

  struct DeviceConfig deviceConfig;
  deviceConfig.m_deviceId = EC_Config.deviceId;
  deviceConfig.m_login = 0;
  deviceConfig.m_password = EC_Config.key;
  deviceConfig.m_hostname = EC_Config.mgt;
  deviceConfig.m_debugLog = debugLog;
  deviceConfig.m_handler = handler;
  deviceConfig.m_storeFields = client.m_storeFields;
  deviceConfig.m_regSize = COUNT_STORE;

  if (!mgt_init(&client, &deviceConfig, &mySocket))
    while (1);

  s1 = mgt_createSignal(&client, "relay", tpBool, SEC_LEV_READ | SIG_ACCESS_READ | SIG_ACCESS_WRITE, STORE_MODE_CHANGE | STORE_UNIT_MIN | 1, 0);
  s2 = mgt_createSignal(&client, "temperature", tpFloat, SEC_LEV_READ | SIG_ACCESS_READ, STORE_MODE_AVERAGE | STORE_UNIT_SEC | 15, 0);
  s3 = mgt_createSignal(&client, "temperature-2", tpFloat, SEC_LEV_READ | SIG_ACCESS_READ, STORE_MODE_AVERAGE | STORE_UNIT_SEC | 15, 0);
  s4 = mgt_createSignal(&client, "min_temperature", tpFloat, SEC_LEV_READ | SIG_ACCESS_READ | SIG_ACCESS_WRITE, STORE_MODE_OFF, 0);
  s5 = mgt_createSignal(&client, "max_temperature", tpFloat, SEC_LEV_READ | SIG_ACCESS_READ | SIG_ACCESS_WRITE, STORE_MODE_OFF, 0);
  s6 = mgt_createSignal(&client, "isAutoMode", tpBool, SEC_LEV_READ | SIG_ACCESS_READ | SIG_ACCESS_WRITE, STORE_MODE_OFF, 0);


  signal_update_double(s4, EC_Config.minTemperature, 0);
  signal_update_double(s5, EC_Config.maxTemperature, 0);
  signal_update_int(s6, EC_Config.isAutoMode, 0);

  mgt_start(&client);

  ticker.attach_ms(125, tick);
}


class SensorTemperature {
    TimeStamp convertTime;
    bool isFind;
    byte rom[8];
    OneWire oneWire;
  public:
    bool online;
    float value;
    SensorTemperature(int aPin) : oneWire(OneWire(aPin)), convertTime(0), online(false), isFind(false)  { }
    // Не блокирующая функция чтения датчика
    // если возращается false, то датчик не получил нового значения
    // если возращается true, то датчик получил новое значение
    bool run(TimeStamp time) {
      if ((time - convertTime) < 800)
        return false; // ещё не готовы читать датчики

      // вычитаем значения датчиков
      if (isFind) { // если датчик уже найден
        oneWire.reset();
        oneWire.select(rom);
        oneWire.write(0xBE);
        byte ram[9];
        for (byte k = 0; k < 9; k++) {
          ram[k] = oneWire.read();
        }
        if (OneWire::crc8(ram, 8) == ram[8]) { // если crc верно
          online = true;
          int divider = 0;
          if (rom[0] == 0x10) // если это 18S20
            divider = 2;
          else if (rom[0] == 0x28) // если это 18B20
            divider = 16;
          else
            Serial.print("Sensor is error type.");
          float temp = (__int16)((ram[1] << 8) | ram[0]);
          value = temp / divider;
        }
        else
          online = false;

        // запустим процесс измерения
        oneWire.reset();
        oneWire.select(rom);
        oneWire.write(0x44, 1);
        convertTime = getUTCTime();
        return online;
      }
      if (oneWire.search(rom)) {
        if (OneWire::crc8(rom, 7) == rom[7]) { // если crc сходится
          isFind = true; // датчик обнаружен
          // запустим процесс измерения
          oneWire.reset();
          oneWire.select(rom);
          oneWire.write(0x44, 1);
          convertTime = getUTCTime();
        }
      }
      return false;

    }
};

struct Period {
  TimeStamp m_interval; // интервал в миллисекундах
  TimeStamp m_last; // установить в 0
};

static bool periodEvent(struct Period* aPeriod, TimeStamp aTime) {
  TimeStamp t = aTime / aPeriod->m_interval;
  if (aPeriod->m_last != t) {
    aPeriod->m_last = t;
    return true;
  }
  return false;
}

struct Period _1_min = { 1L * 60 * 1000, 0 };


void loop() {
  static uint32_t ms1 = 0;
  static uint32_t ms2 = 0;
  static uint32_t ms3 = 0;

  uint32_t ms = millis();
  switch (b1.Loop()) {
    case SB_CLICK :
      relay_on = !relay_on;
      digitalWrite(PIN_RELAY, relay_on);
      break;
    case SB_LONG_CLICK :
      isAP = !isAP;
      if (isAP) blink_mode = 0B00010001;
      WiFi_begin();
      break;
  }

  if (ms < ms2 || (ms - ms2) > 500) {
    ms2 = ms;
    if (isAP == false) {
      if (WiFi.status() == WL_CONNECTED)
        blink_mode = 0B11111111;
      else
        blink_mode = 0B00000101;
    }
  }

  if (ms < ms3 || (ms - ms3) > 5000) {
    ms3 = ms;
    if (isAP == false) {
      if (WiFi.status() != WL_CONNECTED)
        WiFi_begin();
    }
  }

  HTTP_loop();

  static SensorTemperature sensor1(PIN_ONEWIRE_1);
  static SensorTemperature sensor2(PIN_ONEWIRE_2);

  bool relay = digitalRead(PIN_RELAY);

  bool isTemp_1 = false;
  bool isTemp_2 = false;

  TimeStamp t = getUTCTime();

  if (sensor1.run(t)) {     
    isTemp_1 = true;
    if (EC_Config.isAutoMode) {
      if ((sensor1.value < EC_Config.minTemperature) && (!relay)) {
        relay = true;
        digitalWrite(PIN_RELAY, true);
      }
      else if ((sensor1.value > EC_Config.maxTemperature) && relay) {
        relay = false;
        digitalWrite(PIN_RELAY, false);
      }
    }
  }
  else { // если датчик не на связи
    if ((EC_Config.isAutoMode) && relay && (!sensor1.online)) { // если датчик не на связи
      relay = false;
      digitalWrite(PIN_RELAY, false);
    }
  }

  if (sensor2.run(t))
    isTemp_2 = true;

  
  if (!isAP) {
    if (mgt_run(&client) == stConnected) {

      if (periodEvent(&_1_min, t)) {
        signal_update_int(s1, relay, t);
        mgt_send(&client, s1);
      }
      else {
        if (s1->m_value.u.m_bool ^ relay) {
          signal_update_int(s1, relay, t);
          mgt_send(&client, s1);
        }
      }

      if (isTemp_1) {     
        signal_update_double(s2, sensor1.value, t);
        mgt_send(&client, s2);
      }
    
      if (isTemp_2) {
        signal_update_double(s3, sensor2.value, t);
        mgt_send(&client, s3);
      }

    }
  }

}


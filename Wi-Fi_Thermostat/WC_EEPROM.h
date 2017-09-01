/**
* Прошивка SONOFF TH10/16
* Copyright (C) 2016 Алексей Шихарбеев
* http://samopal.pro
*/


#ifndef WC_EEPROM_h
#define WC_EEPROM_h
#include <ESP8266WiFi.h>

extern struct WC_Config EC_Config;

struct WC_Config{
// Параметры в режиме точки доступа
   char name[32]; 
   char password[32];
// Параметры подключения в режиме клиента
   char ssid[32];
   char pass[32];
   IPAddress ip;
   IPAddress msk;
   IPAddress gw;

// Адрес MGT сервера
   char     mgt[48];   
// Идентификатор устройства
   uint32_t deviceId;
// Ключ авторизации
   char key[32];
// режим работы
  bool isAutoMode;
  float minTemperature;
  float maxTemperature;   
// Контрольная сумма   
   uint16_t crc;   
};


void     EC_begin(void);
void     EC_read(void);
void     EC_save(void);

#endif

/**
* Прошивка SONOFF TH10/16
* Copyright (C) 2016 Алексей Шихарбеев
* http://samopal.pro
*/

#include "WC_EEPROM.h"
#include <EEPROM.h>

void debugLog(const __FlashStringHelper* aFormat, ...);

struct WC_Config EC_Config;

/**
 * Инициализация EEPROM
 */
void EC_begin(void){
   size_t sz1 = sizeof(EC_Config);
   EEPROM.begin(sz1);   
   debugLog(F("EEPROM init. Size = %d\n"),(int)sz1);

}

uint16_t EC_crc(void){
  int len = offsetof(WC_Config, crc);
  uint16_t crc = 0;
  for (int i = 0; i < len; i++)
    crc += *((uint8_t*)&EC_Config + i);
  return crc;  
}

// Устанавливаем значения конфигурации по-умолчанию
void EC_default(void){
   memset( &EC_Config, 0, sizeof(EC_Config));
   
   strcpy(EC_Config.name, "Sonoff TH10/TH16");
   strcpy(EC_Config.password, "");
   strcpy(EC_Config.ssid, "none");
   strcpy(EC_Config.pass, "");
   EC_Config.ip[0] = 192;   
   EC_Config.ip[1] = 168;   
   EC_Config.ip[2] = 1;     
   EC_Config.ip[3] = 4;
   EC_Config.msk[0] = 255; 
   EC_Config.msk[1] = 255; 
   EC_Config.msk[2] = 255; 
   EC_Config.msk[3] = 0;
   EC_Config.gw[0] = 192;   
   EC_Config.gw[1] = 168;   
   EC_Config.gw[2] = 1;     
   EC_Config.gw[3] = 1;
   strcpy(EC_Config.mgt, "mgt24.ru");
   EC_Config.deviceId = 0xffffffff;
   strcpy(EC_Config.key, "");
}

// Сохраняем значение конфигурации в EEPROM
void EC_save(void){
   EC_Config.crc = EC_crc();
   for  (int i = 0; i < sizeof(EC_Config); i++)
      EEPROM.write(i, *((uint8_t*)&EC_Config + i));
   EEPROM.commit();     
   debugLog(F("Save Config to EEPROM. SCR=%d\n"),EC_Config.crc);   
}

// Читаем конфигурацию из EEPROM в память
void EC_read(void){
   for (int i = 0; i < sizeof(EC_Config); i++)
       *((uint8_t*)&EC_Config + i) = EEPROM.read(i);
       
    if (EC_Config.crc == EC_crc()) {
       debugLog(F("EEPROM Config is correct\n"));
    }
    else {
       debugLog(F("EEPROM SRC is not valid\n"));
       EC_default();
       EC_save();
    }        
}



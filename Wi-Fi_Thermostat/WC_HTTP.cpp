#include "WC_HTTP.h"
#include "WC_EEPROM.h"

void debugLog(const __FlashStringHelper* aFormat, ...);


ESP8266WebServer server(80);
bool isAP       = false;
bool isConnect  = false;
String authPass = "";
String HTTP_User = "";
int    UID       = -1;


//---Соединение с WiFi----------
static bool ConnectWiFi(void) {

  // Если WiFi не сконфигурирован
  if (strcmp(EC_Config.ssid, "none") == 0) {
     debugLog(F("WiFi is not config ...\n"));
     return false;
  }

  WiFi.mode(WIFI_STA);

  // Пытаемся соединиться с точкой доступа
  debugLog(F("\nConnecting to: %s/%s\n"), EC_Config.ssid, EC_Config.pass);
  WiFi.begin(EC_Config.ssid, EC_Config.pass);
  delay(1000);

  // Максиммум N раз проверка соединения (12 секунд)
  for ( int j = 0; j < 15; j++ ) {
  if (WiFi.status() == WL_CONNECTED) {
      debugLog(F("\nWiFi connect: "));
      //Serial.print(WiFi.localIP());
      //Serial.print("/");
      //Serial.print(WiFi.subnetMask());
      //Serial.print("/");
      //Serial.println(WiFi.gatewayIP());
      return true;
    }
    delay(1000);
    //Serial.print(WiFi.status());
  }
  debugLog(F("\nConnect WiFi failed ...\n"));
  return false;
}

//-------Старт WiFi---------
void WiFi_begin(void){ 
// Подключаемся к WiFi
  if (isAP){
      debugLog(F("Start AP %s\n"),EC_Config.name);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(EC_Config.name, EC_Config.password);
      debugLog(F("Open http://192.168.4.1 in your browser\n"));
  }
  else {
// Получаем статический IP если нужно  
      WiFi.mode(WIFI_STA);
      isConnect = ConnectWiFi(); 
      if( isConnect && EC_Config.ip != 0 ){
         WiFi.config(EC_Config.ip, EC_Config.gw, EC_Config.msk);
         debugLog(F("Open http://"));
         //Serial.print(WiFi.localIP());
         debugLog(F(" in your browser"));
      }
   } 

}


/**
 * Обработчик событий WEB-сервера
 */
void HTTP_loop(void){
  server.handleClient();
}


// Функция проверки пароля
bool HTTP_checkAuth(const char* password) {
  //debugLog(F("password: %s\norig: %s\n"), password, EC_Config.password);
  if (strcmp(password, EC_Config.password) == 0)
    return true;  
  return false;
}

// Проверка авторизации
bool HTTP_isAuth() {
  //Serial.print("AUTH ");
  if (server.hasHeader("Cookie")){   
    //Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    //Serial.print(cookie);
 
    if (cookie.indexOf("password=") != -1) {
      authPass = cookie.substring(cookie.indexOf("password=") + 9);       
      return HTTP_checkAuth((char*)authPass.c_str());
    }
  }
  return false;  
}


char tempBuf[10];
#define EC_STR(str) ;out+=(str);out+=
#define EC_INT(b) ;sprintf(tempBuf,"%lu",(b));out+=tempBuf;out+=

// Страница авторизации
void loginHandler(){
  char* msg = "";
  if (server.hasArg("password")){
    String password = server.arg("password");
    
    if (HTTP_checkAuth(password.c_str())){
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: password=" + password + "\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      debugLog(F("Login Success\n"));
      return;
    }
    msg = "неправильный пароль";
    debugLog(F("Login Failed\n"));
  }
  else {
    if (HTTP_isAuth()) {
      debugLog(F("logout\n"));
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: password=\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      return;          
    }  
  }

  String out =
  F("<html>" "\r\n" 
  "<head>" "\r\n"
  "\t" "<meta charset='utf-8'/>" "\r\n"
  "\t" "<title>Sonoff TH10/TH16</title>" "\r\n"
  "\t" "<style>body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; } input {width:250px; height:28px}</style>" "\r\n"
  "</head>" "\r\n"
  "<body>" "\r\n"
  "<h1>Авторизация</h1>" "\r\n"
  "<form action='/login' method='POST'>" "\r\n"
  "\t" "<input type='password' name='password' placeholder='password' size='32' length='32'>" "\r\n"
  "\t" "<br><br>" "\r\n"
  "\t" "<input type='submit' value='Вход'>" "\r\n"
  "</form>" "\r\n"
  "<h3>") EC_STR(msg) F("</h3>"
  "</body>" "\r\n"
  "</html>" "\r\n");

  server.send(200, "text/html", out);
}

/**
 * Перейти на страничку с авторизацией
 */
void HTTP_gotoLogin(){
  String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
  server.sendContent(header);
}


// Оработчик главной страницы сервера
void mainHandler(void) {
  char str[10];
// Проверка авторизации  

  if (!HTTP_isAuth()) {
    HTTP_gotoLogin();
    return;
  }  

// Сохранение контроллера
  if (server.hasArg("relay")){
    relay_on = !relay_on;
    digitalWrite(PIN_RELAY, relay_on);
    String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }

  char* mode = "";
  if (isAP)
    mode = "Устройство в режиме точки доступа"; 

  char* relayState;
  char* relayCmd;
  if (digitalRead(PIN_RELAY)) {
    relayState = "включено";
    relayCmd = "Выключить";
  }
  else {  
    relayState = "выключено";
    relayCmd = "Включить";
  }

  String out =
  F("<html>" "\r\n"
  "<head>" "\r\n"
  "\t" "<meta charset = \"utf-8\"/>" "\r\n"
  "\t" "<title>") EC_STR(EC_Config.name) F("</title>" "\r\n"
  "\t" "<style>body{ background-color: #dddddd; font-family: Arial, Helvetica, Sans-Serif; color: #000088; }</style>" "\r\n"
  "</head>" "\r\n"
  "<body>" "\r\n"
  "<h1>") EC_STR(EC_Config.name) F("</h1>" "\r\n"
  "<ul>" "\r\n"
  "\t" "<li><a href='/config'>Настройка параметров сети</a></li>" "\r\n"
  "\t" "<li><a href='/login'>Выход</a></li>" "\r\n"
  "</ul>" "\r\n" 
  "<h3>") EC_STR(mode) F("</h3>" "\r\n"
  "<h3>Реле ") EC_STR(relayState) F("</h3>" "\r\n"
  "<form action='/' method='POST'>" "\r\n"
  "\t" "<input type='submit' name='relay' value='") EC_STR(relayCmd) F(" реле'>" "\r\n"
  "</form>" "\r\n"
  "</body>" "\r\n"
  "</html>" "\r\n");

   server.send ( 200, "text/html", out );
}


//-------------------------------------------------
void setConfig() {
// Проверка прав администратора  
  char s[65];
  if (!HTTP_isAuth()) {
    //TODO
    return;
  } 

  if (server.hasArg("name")) strcpy(EC_Config.name, server.arg("name").c_str());
  if (server.hasArg("password")) strcpy(EC_Config.password, server.arg("password").c_str());
  if (server.hasArg("ssid")) strcpy(EC_Config.ssid, server.arg("ssid").c_str());
  if (server.hasArg("pass")) strcpy(EC_Config.pass, server.arg("pass").c_str());
  if (server.hasArg("ip0")) EC_Config.ip[0] = atoi(server.arg("ip0").c_str());
  if (server.hasArg("ip1")) EC_Config.ip[1] = atoi(server.arg("ip1").c_str());
  if (server.hasArg("ip2")) EC_Config.ip[2] = atoi(server.arg("ip2").c_str());
  if (server.hasArg("ip3")) EC_Config.ip[3] = atoi(server.arg("ip3").c_str());
  if (server.hasArg("msk0")) EC_Config.msk[0] = atoi(server.arg("msk0").c_str());
  if (server.hasArg("msk1")) EC_Config.msk[1] = atoi(server.arg("msk1").c_str());
  if (server.hasArg("msk2")) EC_Config.msk[2] = atoi(server.arg("msk2").c_str());
  if (server.hasArg("msk3")) EC_Config.msk[3] = atoi(server.arg("msk3").c_str());
  if (server.hasArg("gw0")) EC_Config.gw[0] = atoi(server.arg("gw0").c_str());
  if (server.hasArg("gw1")) EC_Config.gw[1] = atoi(server.arg("gw1").c_str());
  if (server.hasArg("gw2")) EC_Config.gw[2] = atoi(server.arg("gw2").c_str());
  if (server.hasArg("gw3")) EC_Config.gw[3] = atoi(server.arg("gw3").c_str());
  if (server.hasArg("mgt")) strcpy(EC_Config.mgt, server.arg("mgt").c_str());
  if (server.hasArg("deviceId")) EC_Config.deviceId = strtoul(server.arg("deviceId").c_str(), 0, 0);
  if (server.hasArg("key")) strcpy(EC_Config.key, server.arg("key").c_str());
  EC_save();
  String header = "HTTP/1.1 301 OK\r\nLocation: /config\r\nCache-Control: no-cache\r\n\r\n";
  server.sendContent(header);
  return;   
}

//-------------------------------------------------
void getConfig() {
// Проверка прав администратора  
  char s[65];
  if (!HTTP_isAuth()) {
    HTTP_gotoLogin();
    return;
  } 

	String out =
	F("<html>" "\r\n"
	"<head>" "\r\n"
	"\t" "<meta charset = \"utf-8\"/>" "\r\n"
	"\t" "<title>") EC_STR(EC_Config.name) F("</title>" "\r\n"
	"\t" "<style>body{ background-color: #dddddd; font-family: Arial, Helvetica, Sans-Serif; color: #000088; }</style>" "\r\n"
	"</head>" "\r\n"
	"<body>" "\r\n"
	"\t" "<h1>Настройка параметров сети</h1>" "\r\n"
	"\t" "<ul>" "\r\n"
	"\t\t" "<li><a href='/'>Главная</a></li>" "\r\n"
	"\t\t" "<li><a href='/login'>Выход</a></li>" "\r\n"
	"\t\t" "<li><a href='/reboot'>Перезагрузка</a></li>" "\r\n"
  "\t" "</ul>" "\r\n"
	"\t" "<form action='/config'method='POST'>" "\r\n"
	"\t\t" "<h3>Параметры в режиме точки доступа</h3>" "\r\n"
	"\t\t" "<table>" "\r\n"
	"\t\t\t" "<tr><td>Название:</td><td><input name='name' value='") EC_STR(EC_Config.name) F("' size=32 length=32></td></tr>" "\r\n"
	"\t\t\t" "<tr><td>Пароль:</td><td><input name='password' value='") EC_STR(EC_Config.password) F("' size=32 length=32 type='password'></td></tr>" "\r\n"
	"\t\t" "</table>" "\r\n"
	"\t\t" "<h3>Параметры в режиме клиента</h3>" "\r\n"
	"\t\t" "<table>" "\r\n"
	"\t\t\t" "<tr><td>Имя сети:</td><td><input name='ssid' value='") EC_STR(EC_Config.ssid) F("' size=32 length=32></td></tr>" "\r\n"
	"\t\t\t" "<tr><td>Пароль:</td><td><input name='pass' value='") EC_STR(EC_Config.pass) F("' size=32 length=32 type='password'></td></tr>" "\r\n"
	"\t\t\t" "<tr><td>IP-адрес:</td><td><input name='ip0' value='") EC_INT(EC_Config.ip[0]) F("' size=3 length=3> . <input name='ip1' value='") EC_INT(EC_Config.ip[1]) F("' size=3 length=3> . <input name='ip2' value='") EC_INT(EC_Config.ip[2]) F("' size=3 length=3> . <input name='ip3' value='") EC_INT(EC_Config.ip[3]) F("' size=3 length=3></td></tr>" "\r\n"
	"\t\t\t" "<tr><td>IP-маска:</td><td><input name='msk0' value='") EC_INT(EC_Config.msk[0]) F("' size=3 length=3> . <input name='msk1' value='") EC_INT(EC_Config.msk[1]) F("' size=3 length=3> . <input name='msk2' value='") EC_INT(EC_Config.msk[2]) F("' size=3 length=3> . <input name='msk3' value='") EC_INT(EC_Config.msk[3]) F("' size=3 length=3></td></tr>" "\r\n"
	"\t\t\t" "<tr><td>IP-шлюз:</td><td><input name='gw0' value='") EC_INT(EC_Config.gw[0]) F("' size=3 length=3> . <input name='gw1' value='") EC_INT(EC_Config.gw[1]) F("' size=3 length=3> . <input name='gw2' value='") EC_INT(EC_Config.gw[2]) F("' size=3 length=3> . <input name='gw3' value='") EC_INT(EC_Config.gw[3]) F("' size=3 length=3></td></tr>" "\r\n"
	"\t\t" "</table>" "\r\n"
	"\t\t" "<h3>Подключение к MGT серверу</h3>" "\r\n"
	"\t\t" "<table>" "\r\n"
	"\t\t\t" "<tr><td>Адрес сервера:</td><td><input name='mgt' value='") EC_STR(EC_Config.mgt) F("' size=32 length=32></td></tr>" "\r\n"
  "\t\t\t" "<tr><td>ID устройства : </td><td><input name='deviceId' value='") EC_INT(EC_Config.deviceId) F("' size=32 length=32></td></tr>" "\r\n"
  "\t\t\t" "<tr><td>Ключ авторизации : </td><td><input name='key' value='") EC_STR(EC_Config.key) F("' size=32 length=32></td></tr>" "\r\n"
	"\t\t" "</table>" "\r\n"
	"\t\t" "<br>" "\r\n"
	"\t\t" "<input type = 'submit' value='Сохранить'>" "\r\n"
	"\t" "</form>" "\r\n"
	"</body>" "\r\n"
	"</html>" "\r\n");
	server.send(200, "text/html", out); 
}        



// Перезагрузка
void rebootHandler(void) {
  String out = 
  F("<html>" "\r\n"
  "<head>" "\r\n"
  "\t" "<meta charset='utf-8'/>" "\r\n"
  "\t" "<meta http-equiv='refresh' content='15;URL=/'>" "\r\n"
  "\t" "<title>") EC_STR(EC_Config.name) F("</title>" "\r\n"
  "\t" "<style> body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; } </style>" "\r\n"
  "</head>" "\r\n"
  "<body>" "\r\n"
  "\t" "<h1>Перезагрузка контроллера</h1>" "\r\n"
  "\t" "<p><a href=\"/\">Через 15 сек будет переадресация на главную</a>" "\r\n"
  "</body>" "\r\n"
  "</html>");
  server.send(200, "text/html", out);
  delay(1000);
  ESP.reset();  
}

// Старт WEB сервера
void HTTP_begin(void){
  server.on("/", mainHandler);
  server.on("/config", HTTP_GET, getConfig);
  server.on("/config", HTTP_POST, setConfig);
  server.on("/login", loginHandler);
  server.on("/reboot", rebootHandler);
  //server.onNotFound(notFoundHandler);
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize);
  
  server.begin();
  debugLog(F("HTTP server started ...\n"));
}



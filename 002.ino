/*
 * trying to use esp8266 for coin operated wifi
 * code generator with I2C lcd display.
 * D5 coin slot signal
 * D4 coin slot enable/disable
 * D6 button
 */

#include <Wire.h>
#include <ESP8266TelnetClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LiquidCrystal_I2C.h>

#define CONTROL_PIN 16

ESP8266WebServer server(80);
WiFiClient client;
ESP8266telnetClient tc(client);

#ifndef STASSID
#define STASSID "YourWiFi"  // your Hotspot server
#define STAPSK  ""          // hotspot password were always open
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;

LiquidCrystal_I2C lcd(0x27, 20, 4);  // change this according to your lcd

/* Soft AP network parameters */
//IPAddress apIP(192, 168, 88, 1);
//IPAddress netMsk(255, 255, 255, 0);

//put here your raspi ip address, and login details
IPAddress mikrotikRouterIp (10, 0, 0, 1);  // should be your mikrotik hotspot IP address

// other variables
char tmp[16];
volatile int coinCount = 0, portalTimer = 0;
bool clientToken = false, displayCode = false, debounce = false;
String userName;
int credit = 0, totalTime = 0;
int Day, Hr, Min, Sec;
char mac;
int startTimer = 30;
bool buttonWasPressed = false, display_init = false;

long timer=0;
byte last_sec = 0, second = 0;
int multi = 0;

void ICACHE_RAM_ATTR coinInsert(){
  coinCount++;
  startTimer = 30;
}

//void defaultLoad(){
  //settings.reset();
//}

//void initiateRestart(){
  //ESP.reset();
//}

void setup() {
  // put your setup code here, to run once:
  Serial.begin (115200);
  attachInterrupt(14, coinInsert, RISING);
  pinMode(CONTROL_PIN,OUTPUT);  // D0
  pinMode(2, OUTPUT);     // D2
  pinMode(12, INPUT);     // D6
  digitalWrite(12, HIGH);  // pull up
  digitalWrite(2, LOW);

  //lcd.begin();     // uncomment for some other library that dont work
  lcd.init();
  lcd.backlight();
  idle();

  delay(2000);
  print_txt("Wait for WiFi...", 0, 0);
  Serial.println();
  Serial.print("Wait for WiFi...");


    // this will connect to mikrotik hotspot server

  Serial.println();
  Serial.println("Connecting to WiFi network.");

  WiFi.mode(WIFI_STA);  // station mode
  WiFi.begin(ssid, password); // ssid and password
  delay(1000); // pause for a while
  int count = 0;
  lcd.setCursor(0, 1);
  while ( (WiFi.status() != WL_CONNECTED) && count < 16){  // maximum of 20 attempt
    delay(500);  count++; lcd.print(".");;
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED){ 
    Serial.println("");
    Serial.print("Failed to connect to ");
    Serial.println(ssid);
    Serial.println("Restarting device");
    print_txt("NoWiFi,RESETTING", 0, 1);
    delay(1000);
    ESP.reset();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
  Serial.println("Trying to connect to Mikrotik");

  print_txt(" WiFi Connected ", 0, 1);
  delay(1000);
  print_txt("Connecting to MT", 0, 1);



  //WHICH CHARACTER SHOULD BE INTERPRETED AS "PROMPT"?
  tc.setPromptChar('>');

  delay(500);
  
  //PUT HERE YOUR USERNAME/PASSWORD
  if(tc.login(mikrotikRouterIp, "admin", "admin12345")){        //tc.login(mikrotikRouterIp, "admin", "password", 1234) if you want to specify a port different than 23
    tc.sendCommand("ip");
    tc.sendCommand("address");
    tc.sendCommand("print");
  }
  else{
    Serial.println("Mikrotik login ok");
    print_txt("Connection: OK! ", 0, 1);
    delay(1000);
  }

  // web interface starts here
  server.begin();
  Serial.println("Server started");
  print_txt("Connection: OK! ", 0, 0);
  print_txt("Server Started  ", 0, 1);
  delay(2000);
  idle();
  digitalWrite(CONTROL_PIN, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(millis()-timer >= 1000 || timer > millis()){
    timer = millis();
    second++;
    if(second > 59) second = 0;
  }

  bool insertCoin = digitalRead(12);
  if(!insertCoin){       // button was pressed in idle state
    if(!debounce){
      debounce = true;
      if(displayCode){
        displayCode = false;
        clientToken = false;
        startTimer = 30;
        idle();
      }
      else if(!clientToken){
        print_txt("Insert Coin     ", 0, 0);
        print_txt("                ", 0, 1);
        clientToken = true;
        coinCount = 0;
      }
      else{
        clientToken = false;
        startTimer = 30;
        if(totalTime > 0){    // make sure the total time is not zero, else it will be an open time
          lcd.clear();
          sendMT(totalTime);  // sending code after pressing the button
        }
        else{
          idle();
          displayCode = false;
        }
      }
    }
  }
  else{
    if(debounce){
      debounce = false;
    }
    if(clientToken){
      digitalWrite(2, HIGH);             // enable coin slot
      if(startTimer > 0){
        if(last_sec != second){
          startTimer--;
        }
        print_txt(dtostrf((startTimer), 2, 0, tmp), 14, 0);
        if(coinCount >= 10){                               // rate 1
          multi = 1000;
        }
        else if(coinCount >= 5){                           // rate 2
          multi = 950;
        }
        else{
          multi = 900;                                     // rate 3
        }
        credit = coinCount * multi;                        // product of the coin inserted with multiplier
        totalTime = credit;                                // save it to other variable
        Hr = floor(totalTime / 3600);                      // disect it to days, hours, minutes and seconds
        Min = totalTime / 60;
        Min = Min % 60;
        Sec = totalTime %60;
        Day = floor(Hr / 24);
        print_txt("P=", 0, 1);
        lcd.print(coinCount);
        snprintf(tmp, 16, "%02d:%02d:%02d:%02d", Day, Hr, Min, Sec);
        print_txt(tmp, 5, 1);
      }
      else{
        startTimer = 30;
        clientToken = false;
        if(totalTime > 0){       // make sure the total time is not zero, else it will be an open time
          lcd.clear();
          sendMT(totalTime);
        }
        else{
          idle();
          displayCode = false;
        }
      }
    }
    else{
      digitalWrite(2, LOW);           // disable coinslot
      if(displayCode){
        if(startTimer > 0){
          if(last_sec != second){
            startTimer--;
          }
          print_txt(dtostrf((startTimer), 2, 0, tmp), 14, 1);
        }
        else{
          startTimer = 30;
          displayCode = false;
          lcd.clear();
          idle();
        }
      }
    }
  }

  if(last_sec != second){
    last_sec = second;
  }
}

  void sendMT(int value){
    String data;
    userName = random(1000, 9999);
    data = "ip hotspot user add name="; // "ip hotspot user add name=username limit-uptime=660 profile=vendo server=hotspot1" -> whole terminal command
    data += userName;
    data += " limit-uptime=";
    data += value;
    data += " profile=vendo";         // comment this out if you dont used user profile in your mt
    data += " server=hotspot1";       // comment this out if you dont want this server profile to used in your users
    int datalen = data.length() +1;
    char dataSend[datalen];
    data.toCharArray(dataSend, datalen);
    tc.sendCommand(dataSend);
    print_txt(" <- Your Code -> ", 0, 0);
    lcd.setCursor(6,1);
    lcd.print(userName);
    startTimer = 30;
    displayCode = true;
    /* script that will remove expired user, put this on your user profile(vendo in my case) script
    #removing user from hotspot
    :local x
    :foreach expired in [/ip hotspot user find profile="vendo"] do={
    :if ([/ip hotspot user get $expired uptime]>=[/ip hotspot user get $expired limit-uptime]) do={/ip hotspot user remove $expired}
    }
    >--> end of the script*/
 }

  void print_txt(char temp[16], int x, int y){ // char to display, x,y location
    lcd.setCursor(x,y);
    lcd.print(temp);
  }

  void idle(){
    print_txt("   Welcome To   ", 0, 0);
    print_txt("  Your Hotspot  ", 0, 1);
  }

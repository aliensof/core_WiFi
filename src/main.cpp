
#include <M5Stack.h>
#include <Wire.h>
#include "Adafruit_Sensor.h"
#include <Adafruit_BMP280.h>
#include "SHT3X.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "FS.h"   //--- for use with microSD card
#include <time.h>
#include <Update.h>
#include <M5ez.h>
#include <ezTime.h>
#include <DHT.h>

// ----- Images used in the project ----- //
#include "humidity.c"
#include "temp.c"
// ---------------------------------------

#define FIRMWARE_VERSION  0.3
String THINGSPEAK_WRITE_API_KEY = "HC3DPB783UAXLLMS";
HTTPClient http; // Initialize our HTTP client
const char* server = "http://api.thingspeak.com/update";

#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

uint64_t cardSize = 0;
SHT3X sht30;
Adafruit_BMP280 bmp;

String fileName;
String filedate;
bool iscard = false;
float sht30_humid = 00.0;
float sht30_temp = 00.0;
float bmp_pressure = 00.0;
float bmp_temperature = 00.0;
float dew_point_env = 00.0;
float dht_temp = 00.0;
float dht_humid = 00.0;
float dew_point_dht = 00.0;

/*----User Configurations----*/
String devicename = "Omega3Galil";
float sens_inter = 20;   // sensor interval time in sec, default is 5 Sec
float http_inter = 120; // http send data interval  in sec, default is 120 sec;
bool user_alarm = true;
int speaker_volume = 3;
float temp_upper_limit = -100;
float temp_lower_limit = -100;
float humid_upper_limit = -100;
float humid_lower_limit = -100;

/*----interval rate----*/
unsigned long currentMillis;
static int thingspeak_REFRESH_INTERVAL = http_inter*1000;
static unsigned long thingspeak_RefreshTime = 0;

static int read_interval = sens_inter*1000;
static unsigned long refreshtime = 0;


String updatetime(){
  return String(ez.clock.tz.dateTime("H:i:s"));
}

String updatedate(){
  return String(ez.clock.tz.dateTime("d_M_Y"));
}

/***************************************/
/************GUI INTERFACE**************/
/***************************************/

void mainscreen(){
  ez.screen.clear(0xFFFF);
  ez.canvas.clear();
  ez.header.show(devicename);
  ez.buttons.show("#UserConfig#Settings");
  M5.Lcd.drawBitmap(240, 40, 80, 80, (uint16_t *)temp); 
  M5.Lcd.drawBitmap(225, 140, 80, 80, (uint16_t *)humidity);
}

void settingsscreen(){
  ez.settings.menu();
  mainscreen();
}

void power_saver(){
  ezMenu subMenu("Submenu");
  subMenu.txtSmall();
  subMenu.buttons("up#Back#select##down#");
  subMenu.addItem("Item A");
  subMenu.addItem("Item B");
  subMenu.addItem("Item C");
  subMenu.run();
}

void mobile_connection(){
  ezMenu subMenu("Submenu");
  subMenu.txtSmall();
  subMenu.buttons("up#Back#select##down#");
  subMenu.addItem("Item A");
  subMenu.addItem("Item B");
  subMenu.addItem("Item C");
  subMenu.run();
}

void SensorInterval(){
  String disp_val = "";
  while (true) {
	  if(http_inter > 59){
      disp_val = "Sensor will get Data Every " + String(http_inter/60) + " minutes";
		} 
    else{
			disp_val = "Sensor will get Data Every " + String(http_inter) + " seconds";
    }
		ez.msgBox("Sensor Interval", disp_val, "-#--#ok##+#++", false);
		String b = ez.buttons.wait();
		if(b == "-" && http_inter > 15) http_inter -= 15;
		if(b == "+" && http_inter <= 1800) http_inter += 15;
		if(b == "--"){
		  if(http_inter < 60) http_inter = 15; 
      else http_inter -= 60;
		}
		if(b == "++"){
		  if(http_inter > 1740) http_inter = 1800;
			else http_inter += 60;
		}
		if(b == "ok") break;
  }
  thingspeak_REFRESH_INTERVAL = http_inter*1000;
}

void deviceNaming(){
  devicename = ez.textInput("Set Device Name", "Omega3Galil");
}

void apidata(){
  String THINGSPEAK_WRITE_API_KEY = ez.textInput("Enter thingspeak APIKEY", "L2YDOBGDTBKPCSA3");
}

void AlarmConfig(){
  ezMenu subMenu("Alarm Config");
  subMenu.txtSmall();
  subMenu.buttons("up#Back#select##down#");
  subMenu.addItem("Set Alarm");
  subMenu.addItem("Speaker Volume");
  subMenu.addItem("Upper Temperature Limit");
  subMenu.addItem("Lower Temperature Limit");
  subMenu.addItem("Upper Humidity Limit");
  subMenu.addItem("Lower Humidity Limit");
  subMenu.run();
}

void userconfigscreen(){
  ezMenu userconfig("User Config");
  userconfig.txtSmall();
	userconfig.buttons("up#Back#select##down#");
  // userconfig.addItem("Power Saver", power_saver);
  // userconfig.addItem("Mobile Data", mobile_connection);
  userconfig.addItem("Sensor Read Interval", SensorInterval);
  userconfig.addItem("API KEY", apidata);
  userconfig.addItem("Device Name", deviceNaming);
  userconfig.addItem("Alarm config", AlarmConfig);
  // userconfig.addItem("SD Card");
  userconfig.run();
  mainscreen();
}

/***************************************/

void SendData(float Tin, float Hin, float Pin, float Tdp_in, float dht_t, float dht_h, float Tdp_dht){
  http.begin(server);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Data to send with HTTP POST
  String httpRequestData  = "api_key=";
  httpRequestData += THINGSPEAK_WRITE_API_KEY;
  httpRequestData += "&field1=";
  httpRequestData += String(Tin);
  httpRequestData += "&field2=";
  httpRequestData += String(Hin);
  httpRequestData += "&field3=";
  httpRequestData += String(Pin);
  httpRequestData += "&field4=";
  httpRequestData += String(Tdp_in);
  httpRequestData += "&field5=";
  httpRequestData += String(dht_t);
  httpRequestData += "&field6=";
  httpRequestData += String(dht_h);
  httpRequestData += "&field7=";
  httpRequestData += String(Tdp_dht);

  // Send HTTP POST request
  int httpResponseCode = http.POST(httpRequestData);
  Serial.print("HTTP Response code is: ");
  Serial.println(httpResponseCode);
  http.end();
}

void BMPsensor(){
  if(!bmp.begin(0x76)){
    if(bmp.readPressure() == 121669.81) bmp_pressure = 00.0;
    if(bmp.readTemperature() == -134.82) bmp_temperature = 00.0;
  }
  else{
    bmp_pressure = bmp.readPressure()/100;
    bmp_temperature = bmp.readTemperature();
  }
}

void SHTsensor(){
  if(sht30.get()==0){
    sht30_temp = sht30.cTemp;
    sht30_humid = sht30.humidity;
  }
  else{
    sht30_temp = 00.0;
    sht30_humid = 00.0;
  }
}

void DHTsensor(){
  dht_temp = dht.readTemperature();
  dht_humid = dht.readHumidity();
}

void writeFile(fs::FS &fs, const char *filename, const char *message){
  if(!fs.exists(filename)){
    File file = fs.open(filename, FILE_WRITE);
    if(!file){
      Serial.println("Failed to open file for writing");
      return;
    }
    if(file.print(message)){
      Serial.println("File written");
    }
    else{
      Serial.println("Write failed");
    }
    file.close();
  }
}

void appendFile(fs::FS &fs, const char *path, const char *message){
  Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
    Serial.println("Message appended");
  }
  else{
    Serial.println("Append failed");
  }
  file.close();
}

float dewpoint_calc(float T, float H){
  float Psm = exp((17.368-(T/234.5))*(T/(238.88+T)));
  float Rm = log((H/100)*Psm);
  float dewpoint = ((238.88*Rm)/(17.368-Rm));
  return dewpoint;
}

void initSDcard(){
  if(!SD.begin()){
    Serial.println("Card mount failed");
    iscard = false;
    return;
  }

  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    iscard = false;
    return;
  }

  iscard = true;
  cardSize = SD.cardSize()/(1024*1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  fileName = String("/") + updatedate() + String(".txt");
  filedate = updatedate();
  writeFile(SD, fileName.c_str(), "Time,Temperature[C],Humidity[%],Pressure[Pa]\n");
}

void setup(){
  M5.begin();
  M5.Power.begin();
  Wire.begin();
  ez.begin();
  ez.wifi.begin();
  delay(2000);
  dht.begin();
  delay(2000);
  mainscreen();
  if(WiFi.status()== WL_CONNECTED) initSDcard();
} 

void loop(){
  currentMillis = millis();
  if(millis() - refreshtime >= read_interval){
    refreshtime += read_interval;
    BMPsensor();
    SHTsensor();
    DHTsensor();
    dew_point_dht = dewpoint_calc(dht_temp, dht_humid);
    Serial.printf("DHT temperature: %2.2f[C]  DHT Humidity: %0.2f[%%]  DewPoint: %2.2f[C]\n", dht_temp, dht_humid, dew_point_dht); 
    float temp_median = (sht30_temp + bmp_temperature)/2;
    dew_point_env = dewpoint_calc(temp_median, sht30_humid);
    Serial.printf("temperature: %2.2f[C]  Humidity: %0.2f[%%]  Pressure: %0.2f[mBar]  DewPoint: %2.2f[C]\n", temp_median, sht30_humid, bmp_pressure, dew_point_env); 
    char temp[5];
    char humid[5];
    dtostrf(sht30_temp, 3, 1, temp);
    dtostrf(sht30_humid, 3, 1, humid);
    ez.canvas.font(numonly75);
    ez.canvas.pos(15, 40);
    ez.canvas.print(temp);
    ez.canvas.pos(15, 140);
    ez.canvas.println(humid);
  }

  String button = ez.buttons.poll();
  if (button == "Settings") settingsscreen();
  else if (button == "UserConfig") userconfigscreen();

  if(iscard){
    if(filedate != updatedate()){initSDcard();}
    Serial.println(updatedate());
    String Data = updatetime() +","+String(sht30_temp) +","+ String(sht30_humid) +","+ String(bmp_pressure) +","+ String(dew_point_env) + "\n";
    Serial.println(Data);
    appendFile(SD, fileName.c_str(), Data.c_str());
  }
    
  if(millis() - thingspeak_RefreshTime >= thingspeak_REFRESH_INTERVAL){
    thingspeak_RefreshTime += thingspeak_REFRESH_INTERVAL;
    if(WiFi.status()== WL_CONNECTED){
      // DHTsensor();
      // dew_point_out = dewpoint_calc(dht_temp, dht_humid);
      float temp_median = (sht30_temp + bmp_temperature)/2;
      SendData(temp_median, sht30_humid, bmp_pressure, dew_point_env, dht_temp, dht_humid, dew_point_dht);
    }
  }
  M5.update();
}


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
// #include <DHT.h>

// ----- Images used in the project ----- //
#include "humidity.c"
#include "temp.c"
// ---------------------------------------

#define FIRMWARE_VERSION  0.3
// String THINGSPEAK_WRITE_API_KEY = "L2YDOBGDTBKPCSA3";
String THINGSPEAK_WRITE_API_KEY = "HC3DPB783UAXLLMS";
#define IP "184.106.153.149"
#define PDN_AUTO true

// #define DHTPIN 5
// #define DHTTYPE DHT22
// DHT dht(DHTPIN, DHTTYPE);

uint64_t cardSize = 0;
SHT3X sht30;
Adafruit_BMP280 bmp;

String fileName;
String filedate;
bool iscard = false;
String returndata;
float sht30_humid = 00.0;
float sht30_temp = 00.0;
float bmp_pressure = 00.0;
float bmp_temperature = 00.0;
float dew_point_in = 00.0;
// float dht_temp = 00.0;
// float dht_humid = 00.0;
// float dew_point_out = 00.0;

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

void _readSerial(int t=2500){
  returndata = "";
  delay(t);
  while(Serial2.available()){
    if(Serial2.available()>0) returndata += (char)Serial2.read();
  }
  if(returndata.length() == 0) _readSerial();
  else Serial.println(returndata);
}

void timeInit(){
  Serial2.println("AT+CSNTPSTART=\"jp.ntp.org.cn\"");   // Configure the SNTP server.
  _readSerial();
  Serial2.println("AT+CCLK?");        // Query time
  _readSerial();
  Serial2.println("AT+CSNTPSTOP");    // Stop querying network time
  _readSerial();  
}

void tcpclient(float Tin, float Hin, float Pin, float Tdp_in){
  Serial2.println("AT+CSTT");   // Start task and set APN.
  _readSerial();
  Serial2.println("AT+CIICR");
  _readSerial();
  Serial2.println("AT+CIFSR");
  _readSerial();
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += IP;
  cmd += "\",80";
  Serial2.println(cmd);
  delay(2000);
  if(Serial2.find("Error")){
    Serial.println("AT+CIPSTART error");
    // tcpclient(sht30_temp, sht30_humid, bmp_temperature, bmp_pressure, dew_point_in);
  }

  String getStr = "GET https://api.thingspeak.com/update?api_key=";
  getStr += THINGSPEAK_WRITE_API_KEY;
  getStr += "&field1=";
  getStr += String(Tin);
  getStr += "&field2=";
  getStr += String(Hin);
  getStr += "&field3=";
  getStr += String(Pin);
  getStr += "&field4=";
  getStr += String(Tdp_in);
  // getStr += "&field5=";
  // getStr += String(Tout);
  // getStr += "&field6=";
  // getStr += String(Hout);
  // getStr += "&field7=";
  // getStr += String(Tdp_out);

  cmd = "AT+CIPSEND=";
  cmd += String(getStr.length()+2);
  Serial2.println(cmd);
  delay(5000);
  if(Serial2.find(">")) Serial2.println(getStr);
  else{
    Serial2.println("AT+CIPCLOSE");
    Serial.println("connection closed");
  }
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

void simInit(){
  if(PDN_AUTO == true){
    // --- PDN Auto-activation ---
    Serial.println( "Check SIM Card...\n" );
    Serial2.println("AT+CPIN?");    // Check SIM card status
    _readSerial();
    Serial2.println("AT+CSQ");      // Check RF Signal
    _readSerial();
    // Serial2.println("AT+CGREG=1");   // Check PS service
    // _readSerial();
    Serial2.println("AT+CGREG?");   // Check PS service
    _readSerial(5000);
    Serial2.println("AT+CGACT?");   // Activated automativally
    _readSerial(5000);
    Serial2.println("AT+COPS?");    // Check operator information
    _readSerial(3000);
    Serial2.println("AT+CGCONTRDP"); // Attached PS domain and got IP address automatically
    _readSerial();
  }
  else{
    // --- APN manual configuration --- 
    Serial.println( "Check SIM Card...\n" );
    Serial2.println("AT+CPIN?");    // Check SIM card status
    _readSerial();
    Serial2.println("AT+CFUN=0");   // Disable RF Signal
    _readSerial();
    Serial2.println("AT*MCGDEFCONT=\"IP\",\"internet.golantelecom.net.il\"");   // Set APN manually
    _readSerial();
    Serial2.println("AT+CFUN=1");   // Enable RF
    _readSerial();
    Serial2.println("AT+CGREG?");    // Inquiry PS service.
    _readSerial();
    Serial2.println("AT+CGCONTRDP"); // Attached PS domain and got IP address automatically
    _readSerial();
  }
  // timeInit();
}

void setup(){
  M5.begin();
  M5.Power.begin();
  Wire.begin();
  ez.begin();
  Serial2.begin(115200, SERIAL_8N1, 5, 13);
  ez.wifi.begin();
  delay(1500);
  simInit();
  delay(1500);
  mainscreen();
  if(WiFi.status()== WL_CONNECTED) initSDcard();
} 

void loop(){
  currentMillis = millis();
  if(millis() - refreshtime >= read_interval){
    refreshtime += read_interval;
    BMPsensor();
    SHTsensor();
    float temp_median = (sht30_temp + bmp_temperature)/2;
    dew_point_in = dewpoint_calc(temp_median, sht30_humid);
    Serial.printf("temperature: %2.2f[C]  Humidity: %0.2f[%%]  Pressure: %0.2f[mBar]  DewPoint: %2.2f[C]\n", temp_median, sht30_humid, bmp_pressure, dew_point_in); 
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
    String Data = updatetime() +","+String(sht30_temp) +","+ String(sht30_humid) +","+ String(bmp_pressure) +","+ String(dew_point_in) + "\n";
    Serial.println(Data);
    appendFile(SD, fileName.c_str(), Data.c_str());
  }
  else if(WiFi.status() == WL_CONNECTED && !iscard) initSDcard();
    
  if(millis() - thingspeak_RefreshTime >= thingspeak_REFRESH_INTERVAL){
    thingspeak_RefreshTime += thingspeak_REFRESH_INTERVAL;
    // DHTsensor();
    // dew_point_out = dewpoint_calc(dht_temp, dht_humid);
    float temp_median = (sht30_temp + bmp_temperature)/2;
    // Serial.printf("temperature_out: %2.2f[C]  Humidity_out: %0.2f[%%]  DewPoint_out: %2.2f[C]\n", dht_temp, dht_humid, dew_point_out); 
    tcpclient(temp_median, sht30_humid, bmp_pressure, dew_point_in);
  }
  M5.update();
}

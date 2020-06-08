/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       -
 *    D3       SS
 *    CMD      MOSI
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      SCK
 *    VSS      GND
 *    D0       MISO
 *    D1       -
 */

#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "time.h"
#include "WiFi.h"
#include "WiFiClient.h"
#include "WebServer.h"
#include "ESPmDNS.h"

//BME280 LIBS
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include "Wire.h"

//BME280 SPI
#define BME_SCK 14
#define BME_MISO 12
#define BME_MOSI 13
#define BME_CS 15

#define SEALEVELPRESSURE_HPA (1013.25)

//Adafruit_BME280 bme; I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

float temperature, humidity, pressure, altitude;

//const char* ssid     = "TP-LINK66";
//const char* password = "9255522pas";

const char* ssid     = "KUKUMBA";
const char* password = "123321123";

const int BMEpin = 32;

#define DBG_OUTPUT_PORT Serial

WebServer server(80);

static bool hasSD = false;
File uploadFile;

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}
//load data from SD
bool loadFromSdCard(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  else if(path.endsWith(".txt")) dataType = "application/json";

  digitalWrite(BMEpin, LOW);
  
  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    DBG_OUTPUT_PORT.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
  digitalWrite(BMEpin, LOW);
}

//.....................................................................
// BME280 data to data.txt
File main_folder; // initialize folder for saving
File dataFileBME; // initialize sd file
int prev_file_indx = 0; // used for file naming
String fileName = "data.txt";
//......................................................................

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  digitalWrite(BMEpin, LOW);
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
  digitalWrite(BMEpin, HIGH);
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  digitalWrite(BMEpin, LOW);
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
  digitalWrite(BMEpin, HIGH);
}

void handleFileUpload(){
  digitalWrite(BMEpin, LOW);
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    if(SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile) uploadFile.close();
    DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
  digitalWrite(BMEpin, HIGH);
}

void deleteRecursive(String path){
  digitalWrite(BMEpin, LOW);
  File file = SD.open((char *)path.c_str());
  if(!file.isDirectory()){
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
  digitalWrite(BMEpin, HIGH);
}

void handleDelete(){
  digitalWrite(BMEpin, LOW);
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
  digitalWrite(BMEpin, HIGH);
}

void handleCreate(){
  digitalWrite(BMEpin, LOW);
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
#ifdef ESP8266
      file.write((const char *)0);
#else
      // TODO Create file with 0 bytes???
      file.write(NULL, 0);
#endif
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
  digitalWrite(BMEpin, HIGH);
}

void printDirectory() {
  digitalWrite(BMEpin, LOW);
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
#ifdef ESP8266
    output += entry.name();
#else
    // Ignore '/' prefix
    output += entry.name()+1;
#endif
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 // Send zero length chunk to terminate the HTTP body
 server.sendContent("");
 dir.close();
 digitalWrite(BMEpin, HIGH);
}

void handleNotFound(){
  digitalWrite(BMEpin, LOW);
  if(hasSD && loadFromSdCard(server.uri())) return;
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  DBG_OUTPUT_PORT.print(message);
  digitalWrite(BMEpin, HIGH);
}

void addDataToFile() {
    digitalWrite(BMEpin, HIGH);
    //.................................
    // SD save section
    String data_array = "";
    data_array += String(millis()); // save milliseconds since start of program
    data_array += ",";
    data_array += String(bme.readTemperature()); // save temp
    data_array += ",";
    data_array += String(bme.readHumidity()); // save humidity
    data_array += ",";
    data_array += String(bme.readPressure()/ 133.2F); // save pressure in mm Hg of col
    data_array += ",";
    data_array += String(bme.readAltitude(SEALEVELPRESSURE_HPA)); // save altitude from Adafruit routine

    // SD Card writing and saving
    digitalWrite(BMEpin, LOW);  
    File dataFileBME = SD.open(fileName, FILE_WRITE);
    // if the file is valid, write to it:
    if(dataFileBME){
      dataFileBME.println(data_array);
      dataFileBME.close();
    }
    digitalWrite(BMEpin, HIGH);
    delay(30000);
    //.................................
}

void setup(void){
  
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.setDebugOutput(true);
  DBG_OUTPUT_PORT.print("\n");
  WiFi.begin(ssid, password);
  DBG_OUTPUT_PORT.print("Connecting to ");
  DBG_OUTPUT_PORT.println(ssid);

  pinMode(BMEpin, OUTPUT);

  delay(100);

  bme.begin(0x76);

    //.................................
    // verify SD is working
    digitalWrite(BMEpin, LOW);
    bool status;
    status = SD.exists(fileName);
    if (!status) {
      DBG_OUTPUT_PORT.print("File " + fileName + " not founded!");
    }
    else {
      DBG_OUTPUT_PORT.print("File " + fileName + " exists!");
    }
    digitalWrite(BMEpin, HIGH);
    //.................................
     
  // Wait for connection
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {//wait 10 seconds
    delay(500);
  }
  if(i == 21){
    DBG_OUTPUT_PORT.print("Could not connect to ");
    DBG_OUTPUT_PORT.println(ssid);
    while(1) delay(500);
  }
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){ returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

  digitalWrite(BMEpin, LOW);
  if (SD.begin(SS)){
     DBG_OUTPUT_PORT.println("SD Card initialized.");
     hasSD = true;
  }
  digitalWrite(BMEpin, HIGH);
}

void printValues() {
  digitalWrite(BMEpin, HIGH);
  Serial.print("Temperature = ");
  Serial.print(bme.readTemperature());
  Serial.println(" *C");
  
  Serial.print("Pressure = ");
  Serial.print(bme.readPressure() / 133.2F);
  Serial.println(" mm Hg column");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.print("Humidity = ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.println();
  digitalWrite(BMEpin, LOW);
  delay(30000);
}

void loop(void){
  server.handleClient();
  digitalWrite(BMEpin, HIGH);  //  включаем светодиод
                             //  (HIGH – это значение напряжения)
  delay(1000);                 // ждем секунду
  digitalWrite(BMEpin, LOW);   //  выключаем светодиод,
                             //  выставляя напряжение на LOW
  delay(30000);

  //.............................................................................
  
  //.............................................................................
  printValues();
  addDataToFile();
}

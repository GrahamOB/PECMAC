// Distributed with a free-will license.
// Use it any way you want, profit or free, provided it fits in the licenses of its associated works.
// PECMAC125A
// This code is designed to work with the PECMAC125A_DLCT03C20 I2C Mini Module available from ControlEverything.com.
// https://www.controleverything.com/content/Current?sku=PECMAC125A_DLCT03C20#tabs-0-product_tabset-2

#include<Wire.h>

// PECMAC125A I2C address is 2A(42)
#define Addr 0x2A
#include <InfluxDbClient.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define WIFI_SSID "GBUS-MESH"
#define WIFI_PASSWORD "98manager"
#define INFLUXDB_URL "http://192.168.10.50:8086"
#define INFLUXDB_DB_NAME "powerstat"
#define DEVICE "BQ27441"
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);
Point sensor("powerstat");

unsigned int data[36];
int typeOfSensor = 0;
int maxCurrent = 0;
int noOfChannel = 0;

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  Serial.println("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  WiFi.hostname("PECMON");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);}
  ArduinoOTA.setHostname("OTAESP8266");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready!!!!!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
    

  // Start I2C transmission
  Wire.beginTransmission(Addr);
  // Command header byte-1
  Wire.write(0x92);
  // Command header byte-2
  Wire.write(0x6A);
  // Command 2 is used to read no of sensor type, Max current, No. of channel
  Wire.write(0x02);
  // Reserved
  Wire.write(0x00);
  // Reserved
  Wire.write(0x00);
  // Reserved
  Wire.write(0x00);
  // Reserved
  Wire.write(0x00);
  // CheckSum
  Wire.write(0xFE);
  // Stop I2C transmission
  Wire.endTransmission();

  // Request 6 bytes of data
  Wire.requestFrom(Addr, 6);

  // Read 6 bytes of data
  if (Wire.available() == 6)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
    data[2] = Wire.read();
    data[3] = Wire.read();
    data[4] = Wire.read();
    data[5] = Wire.read();
  }

  typeOfSensor = data[0];
  maxCurrent = data[1];
  noOfChannel = data[2];

  // Output data to serial monitor
  Serial.print("Type Of Sensor : ");
  Serial.println(typeOfSensor);
  Serial.print("Max Current : ");
  Serial.print(maxCurrent);
  Serial.println(" Amp");
  Serial.print("No. Of Channel : ");
  Serial.println(noOfChannel);
  delay(300);
}

void loop()
{
  ArduinoOTA.handle();
  Wire.beginTransmission(Addr);
  Wire.write(0x92);
  Wire.write(0x6A);
  Wire.write(0x01);
  Wire.write(1);
  Wire.write(1);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write((0x92 + 0x6A + 0x01 + 1 + 1 + 0x00 + 0x00) & 0xFF);
  Wire.endTransmission();
  delay(500);
  Wire.requestFrom(Addr, 3);
  int msb1 = Wire.read();
  int msb = Wire.read();
  int lsb = Wire.read();
  float current1 = (msb1 * 65536) + (msb * 256) + lsb;
  float ch1current = current1 / 10000;

  Wire.beginTransmission(Addr);
  Wire.write(0x92);
  Wire.write(0x6A);
  Wire.write(0x01);
  Wire.write(2);
  Wire.write(2);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write((0x92 + 0x6A + 0x01 + 2 + 2 + 0x00 + 0x00) & 0xFF);
  Wire.endTransmission();
  delay(500);
  Wire.requestFrom(Addr, 3);
  int msb2 = Wire.read();
  int msb3 = Wire.read();
  int lsb1 = Wire.read();
  float current2 = (msb2 * 65536) + (msb3 * 256) + lsb1;
  float ch2current = current2 / 10000;
  
  float chtotali = (ch1current + ch2current);

  // Output to the serial monitor
  Serial.print("Channel : ");
  Serial.println(0);
  Serial.print("Current Value : ");
  Serial.println(ch1current);

  Serial.print("Channel : ");
  Serial.println(1);
  Serial.print("Current Value : ");
  Serial.println(ch2current);
//  delay(1000);

  sensor.clearFields();
  sensor.addField("channel1i", ch1current);
  sensor.addField("channel2i", ch2current);
  sensor.addField("chtotali", chtotali);
  sensor.addField("rssi", WiFi.RSSI());
  sensor.addField("sensortype", typeOfSensor);
  sensor.addField("maxCurrent", maxCurrent);
  sensor.addField("noOfChannel", noOfChannel);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensor));
  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
}

//   delay(10000);

}

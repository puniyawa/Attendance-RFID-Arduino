#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPSRedirect.h>

#include<Wire.h>
#include<LiquidCrystal_I2C.h>

#define total_blocks  (sizeof(blocks) / sizeof(blocks[0]))
#define RST_PIN  0  //D3
#define SS_PIN   2  //D4
#define BUZZER   4  //D2

MFRC522 mfrc522(SS_PIN, RST_PIN);        // Reference RFID PCV Scanner
MFRC522::MIFARE_Key key;  
MFRC522::StatusCode status;

LiquidCrystal_I2C lcd(0x27, 16, 2);      // Reference LCD Display

// ======================== SETTINGS ========================

// Enter Google Script Deployment ID:
const char *GScriptId = "AKfycbzMx3apYeijh7VqqkneBIMS2aSS2PctnfIMHnR2rmOfT5e1WUfp2J_lAbP0m82oFnk";

// Enter WIFI:
const char* ssid     = "PLDTHOMEFIBRb8c30";
const char* password = "PLDTWIFIx9kzh";

/*          OPTIONS           */
// Show WIFI name in the LCD
const bool showSSID = true;
// How many reconnection attempts
const int reconAttemps = 5;
// Greetings
const char* greetingsMsg = "Dangal Greetings";

// ==========================================================
String gate_number = "Gate1";
String payload_base =  "{\"command\": \"insert_row\", \"sheet_name\": \"Sheet3\", \"values\": ";
String payload = "";

const char* host        = "script.google.com";
const int   httpsPort   = 443;
const char* fingerprint = "";

String url = String("/macros/s/") + GScriptId + "/exec";
HTTPSRedirect* client = nullptr;

String student_id;

int blocks[] = {4,5,6,8,9};


int blockNum = 2;  

byte bufferLen = 18;
byte readBlockData[18];

void setup() {
  Serial.begin(9600);        
  delay(10);
  Serial.println('\n');

  SPI.begin();

  InitLCD();            // Initialize the LCD
  ConnectWifi();        // Connect to Wifi
  ConnectToGoogle();    // Connect to Google
}

void loop() {
  static bool flag = false;
  if (!flag){
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  
  // If disconnected
  if (client != nullptr){
    if (!client->connected()){
      int retval = client->connect(host, httpsPort);
      if (retval != 1){
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Disconnected.");
        lcd.setCursor(0,1); 
        lcd.print("Retrying...");
        return; // Exit
      }
    }
  }

  // Scanning for RFID CARD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scan your card");

  mfrc522.PCD_Init();
  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(1000);
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(1000);
    return;
  }

  // Read Data from RFID Card
  String values = "", data;

  for (byte i = 0; i < total_blocks; i++) {
    ReadDataFromBlock(blocks[i], readBlockData);
    if(i == 0){
      data = String((char*)readBlockData);
      data.trim();
      student_id = data;
      values = "\"" + data + ",";
    }
    else{
      data = String((char*)readBlockData);
      data.trim();
      values += data + ",";
    }
  }
  // User values to be uploaded to Google Sheets [ JSON ]
  values += gate_number + "\"}";
  payload = payload_base + values;

  //----------------------------------------------------------------
  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print("Writing Data");
  lcd.setCursor(0,1); 
  lcd.print("to Google Sheets...");
  //----------------------------------------------------------------
  // Publish data to Google Sheets
  if(client->POST(url, host, payload)){ 
    lcd.clear();
    lcd.setCursor(0,0); 
    lcd.print(greetingsMsg);
    lcd.setCursor(0,1); 
    lcd.print("ID:" + student_id);

    delay(2500);

    lcd.clear();
    lcd.setCursor(0,0); 
    lcd.print("Login Success.");
  }
  else{
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Failed.");
    lcd.setCursor(0,1); 
    lcd.print("Try Again");
  } 
  delay(1000);
}

//
//   Initialiation methods 
//

/*           LCD             */

void InitLCD(){
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

/*      WIFI AND NETWORK      */
void ConnectWifi(){
  // Connect to WiFi
  WiFi.begin(ssid, password);

  lcd.setCursor(0,0);
  lcd.print("Connecting to ");
  lcd.setCursor(0,1);
  if (showSSID){
    lcd.print(ssid);
  }
  else{
    lcd.print("WiFi...");
  }
  delay(5000);
}

void ConnectToGoogle(){
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");

  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print("Connecting to");
  lcd.setCursor(0,1);
  lcd.print("Google ");
  delay(5000);

  // Connect with Google server reconAttemps times
  bool isConnected = false;
  for(int i=0; i < reconAttemps; i++){ 
    int retval = client->connect(host, httpsPort);
    if (retval == 1){
      isConnected = true;
      lcd.clear();
      lcd.setCursor(0,0); 
      lcd.print("Connected to");
      lcd.setCursor(0,1); 
      lcd.print("Google Sheets");
      delay(2000);
      break;
    }
    else
      lcd.setCursor(0,0); 
      lcd.print("Connection failed. Retrying...");
  }
  
  // If connection is past reconAttemps times.
  if (!isConnected){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connection failed");
    lcd.setCursor(0,1);
    lcd.print("Cannot be reached.");
    
    delay(5000);
    return;
  }
  
  // Terminate
  delete client;    
  client = nullptr; 
}

//
//   Reading and Encoding methods 
//

/*       READING DATA        */
void ReadDataFromBlock(int blockNum, byte readBlockData[]) 
{ 
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK){
     Serial.print("Authentication failed for Read: ");
     Serial.println(mfrc522.GetStatusCodeName(status));
     return;
  }
   else {
    Serial.println("Authentication success");
  }
  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else {
    readBlockData[16] = ' ';
    readBlockData[17] = ' ';
  }
}

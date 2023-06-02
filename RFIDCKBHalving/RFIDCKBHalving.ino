#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Time.h>

#define WIFI_SSID "YOUR SSID"
#define WIFI_PASSWORD "YOUR PASSWORD"
#define RST_PIN 4                                     // Configurable
#define SS_PIN 5                                      // Configurable
#define serverName "https://rpc.ankr.com/nervos_ckb"  // Alternate "https://ckb.getblock.io/7f002c45-22bf-4675-8f7f-55beb43bc192/mainnet/";
#define WTAPI "http://worldtimeapi.org/api/timezone/" // Time Server Lookup
#define RFID_UID  "YOUR UID"                          // UID of trigger card
#define TEXT_BRIGHT_GREEN 0x0D21
#define TEXT_ORANGE 0xAB22
#define TEXT_PALE_BLUE 0x63B9

JsonObject object;
MFRC522 mfrc522(SS_PIN, RST_PIN);   
ESP32Time rtc;
TFT_eSPI tft = TFT_eSPI();

typedef String HexString;

const int freq = 5000;
const int tftledChannel = 0;
const int resolution = 8;
long clockTimer = millis();
long CUR_TIME_OFFSET = 0;
long RESET_TIMER = 0;
String TZSTRING = "";
String timeRemaining;
int CUR_HOUR = -1;
int CUR_MIN = -1;
int CUR_SEC = -1;
bool HALVING_CHECK = false;

String getHalving();
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void GET_TIMEZONE(String REGION_, String CITY_);
HexString byteArrayToHex(const uint8_t* byteArray, size_t len);
String dateFormat(int DAY_, int MONTH_);

void setup() {
  pinMode(TFT_BL, OUTPUT);      //  TFT_BL declared in TFT_eSPI user setup file
  ledcSetup(tftledChannel, freq, resolution);
  ledcAttachPin(TFT_BL, tftledChannel);
  ledcWrite(tftledChannel, 200);
  Serial.begin(9600);  // Initialize serial communications with the PC
  while (!Serial)
    ;           // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();  // Init SPI bus

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield();  // Stay here twiddling thumbs waiting
  }
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLUE);
  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(WiFi.localIP());

  GET_TIMEZONE("Australia", "Adelaide");    //Enter Your Country/Region and City

  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  TJpgDec.drawFsJpg(0, 0, "/clock01.jpeg");
  tft.setCursor(113, 50);
  tft.print(":");
  tft.setCursor(192, 50);
  tft.print(":");
  mfrc522.PCD_Init();                 // Init MFRC522
  delay(4);                           // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}

void loop() {
  if (millis() - clockTimer > 1000) {
    if (!HALVING_CHECK) {
      updateClock();
      clockTimer = millis();
    } else {
      TJpgDec.drawFsJpg(0, 0, "/clock01.jpeg");
      CUR_HOUR = -1;
      CUR_MIN = -1;
      updateClock();
      clockTimer = millis();
      HALVING_CHECK = false;
    }
  }


  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Dump debug info about the card; PICC_HaltA() is automatically called

  String CARDHEX = byteArrayToHex(mfrc522.uid.uidByte, mfrc522.uid.size);
  if (CARDHEX != RFID_UID) {
    tft.fillRect(54, 164, 210, 50, TFT_BLACK);
    tft.setCursor(60, 180);
    tft.loadFont("JMHTypewriter30");
    tft.setTextColor(TEXT_ORANGE);
    tft.print(CARDHEX);
    tft.unloadFont();
  } else {
    tft.fillRect(56, 32, 210, 80, TFT_BLACK);
    tft.fillRect(45, 40, 233, 65, TFT_BLACK);
    TJpgDec.drawFsJpg(56, 32, "/hh01.jpeg");
    tft.fillRect(47, 164, 230, 50, TFT_BLACK);
    tft.setCursor(65, 185);
    tft.loadFont("JMHTypewriter20");
    tft.setTextColor(TEXT_ORANGE);
    tft.print("Hello Phillip.bit");
    Serial.println(getHalving());
    tft.unloadFont();
    clockTimer = -65000;
    HALVING_CHECK = true;
  }
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
}

String getHalving() {
  DynamicJsonDocument doc(60000);
  DynamicJsonDocument jsonRes(48596);
  String retOut = "";
  long curBlock = 0;
  String jsOut = "";
  DeserializationError error;
  long cLength;
  long cStart;
  long cTarget;
  long cEpoch;
  long cHalfTarget;
  long cBlockTip;

  object = doc.to<JsonObject>();
  object["id"] = 0;
  object["jsonrpc"] = "2.0";
  object["method"] = "get_current_epoch";
  object["params"];

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String serverPath = String(serverName);
    serializeJson(doc, jsOut);
    Serial.println(serverPath);
    http.begin(serverPath);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsOut);
    jsOut = "\0";
    if (httpResponseCode > 0) {
      String payload = http.getString();
      error = deserializeJson(jsonRes, payload);
      if (error) {
        Serial.println(error.c_str());
        return (error.c_str());
      }
      if (jsonRes["error"]["code"] == -32603) {
        retOut = "Server Error";
      } else {
        JsonObject result = jsonRes["result"];
        int id = doc["id"];                                            // 42
        const char* jsonrpc = doc["jsonrpc"];                          // "2.0"
        const char* result_compact_target = result["compact_target"];  // "0x1e083126"
        const char* result_length = result["length"];                  // "0x708"
        const char* result_number = result["number"];                  // "0x1"
        const char* result_start_number = result["start_number"];      // "0x3e8"
        char* endptr;
        cLength = strtol(result_length, &endptr, 16);
        cStart = strtol(result_start_number, &endptr, 16);
        cTarget = cStart + cLength;
        cEpoch = strtol(result_number, &endptr, 16);
      }
    }
    delay(100);
    object = doc.to<JsonObject>();
    object["id"] = 0;
    object["jsonrpc"] = "2.0";
    object["method"] = "get_consensus";
    object["params"];

    serializeJson(doc, jsOut);
    httpResponseCode = http.POST(jsOut);
    jsOut = "\0";

    if (httpResponseCode > 0) {
      Serial.println(httpResponseCode);
      String payload = http.getString();
      error = deserializeJson(jsonRes, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return (error.c_str());
      }
      if (jsonRes["error"]["code"] == -32603) {

        retOut = "Server Error";
      } else {
        JsonObject result = jsonRes["result"];

        int id = doc["id"];                    // 42
        const char* jsonrpc = doc["jsonrpc"];  // "2.0"
        const char* result_primary_epoch_reward_halving_interval = result["primary_epoch_reward_halving_interval"];
        char* endptr;
        cHalfTarget = strtol(result_primary_epoch_reward_halving_interval, &endptr, 16);
      }
    }
    delay(100);
    object = doc.to<JsonObject>();
    object["id"] = 0;
    object["jsonrpc"] = "2.0";
    object["method"] = "get_tip_block_number";
    object["params"];

    serializeJson(doc, jsOut);

    httpResponseCode = http.POST(jsOut);
    jsOut = "\0";

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      error = deserializeJson(jsonRes, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return (error.c_str());
      }
      if (jsonRes["error"]["code"] == -32603) {

        retOut = "Server Error";
      } else {

        const char* result = jsonRes["result"];  // "0x400"
        char* endptr;
        String blockDec = String(result, 10);
        curBlock = blockDec.toFloat();
      }
    }
  }
  float fTarget = float(cTarget);
  float fCurBlock = float(curBlock);
  float fLength = float(cLength);
  float partEpoch = (fTarget - fCurBlock) / fLength * 14400;
  unsigned long timeToGo = (cHalfTarget - cEpoch) * 14400 + partEpoch;
  int dayRemain = int(timeToGo / 86400);
  int hoursRemain = int((timeToGo / 3600) - (dayRemain * 24));
  int minsRemain = int((timeToGo / 60) - (dayRemain * 1440) - (hoursRemain * 60));
  int secsRemain = int(timeToGo - (dayRemain * 86400) - (hoursRemain * 3600) - (minsRemain * 60));
  timeRemaining = String(dayRemain) + " days, " + String(hoursRemain) + " hours, \n      " + String(minsRemain) + " minutes and " + String(secsRemain) + " seconds";
  unsigned long curUnix = rtc.getEpoch();
  Serial.println(curUnix);
  Serial.println(rtc.getDateTime(true));
  unsigned long newTime = curUnix + timeToGo;
  Serial.println(newTime);
  int curYEAR = rtc.getYear();
  int curDOY = rtc.getDayofYear();
  rtc.setTime(newTime);
  retOut = rtc.getDateTime(true);
  tft.setCursor(65, 168);
  tft.loadFont("JMHTypewriter20");
  tft.fillRect(54, 164, 210, 50, TFT_BLACK);
  tft.print(timeRemaining);
  tft.unloadFont();
  GET_TIMEZONE("Australia", "Adelaide");
  return (retOut);
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height()) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

HexString byteArrayToHex(const uint8_t* array, size_t len) {
  String hexStr = "";
  for (size_t i = 0; i < len; i++) {
    hexStr += String(array[i], HEX);
  }
  return hexStr;
}

void updateClock() {

  tft.loadFont("DS-DIGI70");
  tft.setTextSize(1);
  tft.setTextColor(TEXT_BRIGHT_GREEN);
  int HOUR_ = rtc.getHour(true);
  int MINUTE_ = rtc.getMinute();
  int SECOND_ = rtc.getSecond();
  String DATE_STRING = String(rtc.getDayofWeek()) + ", " + String(rtc.getMonth()) + " " + String(rtc.getDay()) + ", " + String(rtc.getYear());
  Serial.println(String(HOUR_) + " " + String(MINUTE_) + " " + String(SECOND_));
  Serial.println(String(CUR_HOUR) + " " + String(CUR_MIN) + " " + String(CUR_SEC));
  int X_VAL = 46;
  if (HOUR_ != CUR_HOUR) {
    tft.fillRect(49, 35, 68, 78, TFT_BLACK);
    tft.fillRect(49, 180, 220, 30, TFT_BLACK);
    tft.setCursor(113, 50);
    tft.print(":");
    if (HOUR_ > 9 && HOUR_ < 20) {
      if(HOUR_ == 11){
        X_VAL += 8;
      }
      X_VAL += 8;
    }else if (HOUR_ == 1||HOUR_ == 21) {
      X_VAL += 8;
    }
    tft.setCursor(X_VAL, 50);
    if (HOUR_ < 10) {
      tft.print("0" + String(HOUR_));
      CUR_HOUR = HOUR_;
    } else {
      tft.print(HOUR_);
      CUR_HOUR = HOUR_;
    }

    tft.setCursor(50, 180);
    tft.unloadFont();
    tft.setTextColor(TEXT_PALE_BLUE);
    tft.loadFont("JMHTypewriter20");
    tft.print(dateFormat(rtc.getDayofWeek(), rtc.getMonth()));
    tft.unloadFont();
    tft.loadFont("DS-DIGI70");
    tft.setTextColor(TEXT_BRIGHT_GREEN);
  }
  X_VAL = 125;
  if (MINUTE_ > 9 && MINUTE_ < 20) {
    if(MINUTE_ == 11){
        X_VAL += 8;
      }
    X_VAL += 8;
  }else if (MINUTE_ == 1||MINUTE_ == 21||MINUTE_ == 31||MINUTE_ == 41||MINUTE_ == 51) {
    X_VAL += 8;
  }
  
  if (MINUTE_ != CUR_MIN) {
    tft.fillRect(123, 35, 74, 78, TFT_BLACK);
    tft.setCursor(192, 50);
    tft.print(":");
    tft.setCursor(X_VAL, 50);
    if (MINUTE_ < 10) {
      tft.print("0" + String(MINUTE_));
      CUR_MIN = MINUTE_;
    } else {
      tft.print(MINUTE_);
      CUR_MIN = MINUTE_;
    }
  }
  X_VAL = 203;
  if (SECOND_ > 9 && SECOND_ < 20) {
    if(SECOND_ == 11){
      X_VAL += 8;  
    }
    X_VAL += 8;
  }
  if (SECOND_ != CUR_SEC) {
    tft.fillRect(202, 38, 73, 67, TFT_BLACK);
    tft.setCursor(X_VAL, 50);
    if (SECOND_ < 10) {
      tft.print("0" + String(SECOND_));
      CUR_SEC = SECOND_;
    } else {
      tft.print(SECOND_);
      CUR_SEC = SECOND_;
    }
  }

  tft.unloadFont();

  Serial.println(time(nullptr));
}

//Timezone Offset Lookup using WorldTimeApi
void GET_TIMEZONE(String REGION_, String CITY_) {
  DeserializationError error;
  DynamicJsonDocument TZJ(1000);
  String jsOut = "";
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String serverPath = String(WTAPI) + REGION_ + "/" + CITY_;
    Serial.println(serverPath);
    http.begin(serverPath);
    int httpResponseCode = http.GET();
    jsOut = "\0";

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      //Serial.println(payload);
      error = deserializeJson(TZJ, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      }

      const char* abbreviation = TZJ["abbreviation"];  // "ACST"
      int dst_offset = TZJ["dst_offset"];  // 0
      long raw_offset = TZJ["raw_offset"];     // 34200
      const char* timezone = TZJ["timezone"];  // "Australia/Adelaide"
      long unixtime = TZJ["unixtime"];         // 1682070865
      const char* utc_offset = TZJ["utc_offset"];  // "+09:30"

      CUR_TIME_OFFSET = raw_offset;
      TZSTRING = String(abbreviation) + " " + String(utc_offset) + " utc";
      Serial.println(raw_offset);
      Serial.println(CUR_TIME_OFFSET);
      rtc.setTime(unixtime + CUR_TIME_OFFSET);
    }
  }
}

String dateFormat(int DAY_, int MONTH_) {
  String RET_DATE = "";
  switch (DAY_) {
    case 1:
      RET_DATE += "Monday, ";
      break;

    case 2:
      RET_DATE += "Tueday, ";
      break;

    case 3:
      RET_DATE += "Wednesday, ";
      break;

    case 4:
      RET_DATE += "Thursday, ";
      break;

    case 5:
      RET_DATE += "Friday, ";
      break;

    case 6:
      RET_DATE += "Saturday, ";
      break;

    case 7:
      RET_DATE += "Sunday, ";
      break;
  }

  switch (MONTH_) {
    case 0:
      RET_DATE += "January ";
      break;

    case 1:
      RET_DATE += "February ";
      break;

    case 2:
      RET_DATE += "March ";
      break;

    case 3:
      RET_DATE += "April ";
      break;

    case 4:
      RET_DATE += "May ";
      break;

    case 5:
      RET_DATE += "June ";
      break;

    case 6:
      RET_DATE += "July ";
      break;

    case 7:
      RET_DATE += "August ";
      break;

    case 8:
      RET_DATE += "September ";
      break;

    case 9:
      RET_DATE += "October ";
      break;

    case 10:
      RET_DATE += "November ";
      break;

    case 11:
      RET_DATE += "December ";
      break;
  }

  RET_DATE += String(rtc.getDay());

  return RET_DATE;
}

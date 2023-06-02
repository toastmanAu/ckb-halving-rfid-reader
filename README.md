# ckb-halving-rfid-reader
Esp32 clock with RFID activated CKB halving calculator

Uses an ESP32 Devkit, a 2.8" TFT LCD display (ILI9341 used here but would work on any 320x240 supported by TFT_eSPI) and an MFRC-522 RFID reader/writer module. The sketch is written on the Arduino IDE but should work on platform IO also. 

Parts List
-ESP32 Devkit
-ILI9341 TFT Display
-MFRC-522 RFID module
-bunch of jumpers
-3 x 3 way terminal blocks for shared 3.3v, Neutral and Reset connections 

Library Dependancies
-Arduino (if using platform.io only)
-WiFi
-WiFiClientSecure
-TFT_eSPI
-SPI
-MFRC522
-HTTPClient
-ArduinoJson
-ESP32Time

You'll need to Update your user_setup.h file for TFT_eSPI to utilise the Hardware SPI (HSPI) bus, by default the Virtual SPI (VSPI) bus is utilised but in this instance i've wired the SPI controlled RFID reaer to the VSPI bus and the SPI controlled display the HSPI.

There are several user entries required in the code for setup, namely your SSID/Password, your Region/Country and City and the UID of your trigger card/tag. I'm using a custom Nervos RFID card i got from a now defunct project but you could just as easily modify the code to be triggered by any RFID device compatible with the MFRC522, they appear to work for at lease mifare classic and ultralight cards. The ultra lights can have some issues with this reader but forthe purpose of this project we need only the UID from the card rather than any other data records. To obtain your UID either use the card dump example of the MFRC library or if your not sure how/unable to, you can use an RFID reading app on most smartphones (any with NFC tech).

The code utilises the internal SPIFFS file system for storage of fonts and images. You need copy the project folder in its entirety (sketch and "data" folder and it's contents) Then you need to upload the generated SPIFFS image using either an Arduino Plugin or ESPTOOL manually.

/****************************************************************************
 * Bridge to transparent send UART over PCA9685
 * The idea is to send one-way bootloader over PCA9685 
 ****************************************************************************/

#include <Wire.h>
#include <SoftwareSerial.h>

//Pins used
#define LOAD_SWITCH_PIN   6
#define UART_TX_PIN       2

//PCA9658 settings.
#define PCA9685_ALL_CALL 0x70
#define MODE1 0x00
#define MODE2 0x01
#define LED0_ON_L 0x06

// SoftwareSerial handles the timing on Pin 2
SoftwareSerial pcaUart(-1, UART_TX_PIN); 

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  //Host PC shall use 2 stopbits to give this time to forward the chars.
  Serial.begin(9600);

  Wire.begin();
  Wire.setClock(100000);
  
  pinMode(UART_TX_PIN, OUTPUT);
  digitalWrite(UART_TX_PIN, HIGH); 

  // 2. Setup PCA9685 MODE2
  Wire.beginTransmission(PCA9685_ALL_CALL);
  Wire.write(MODE2);
  // 0x04 (Push-Pull) | 0x01 (When OE=1, output is High/Idle)
  Wire.write(0x05); 
  Wire.endTransmission();

  // 3. Wake up
  Wire.beginTransmission(PCA9685_ALL_CALL);
  Wire.write(MODE1);
  Wire.write(0x01); 
  Wire.endTransmission();
  delay(5);

  // 4. Set all channels to "Full ON"
  // This ensures that when OE is LOW, the output is driven LOW (Start bit)
  for (uint8_t i = 0; i < 16; i++) {
    Wire.beginTransmission(PCA9685_ALL_CALL);
    Wire.write(LED0_ON_L + (i * 4));
    Wire.write(0x00); Wire.write(0x10); 
    Wire.write(0x00); Wire.write(0x00); 
    Wire.endTransmission();
  }
  delay(10);

  //Turn on LOAD switch
  pinMode(LOAD_SWITCH_PIN, OUTPUT);
  digitalWrite(LOAD_SWITCH_PIN, LOW); 
  delay(50);

  //Init software uart.
  pcaUart.begin(9600); 

  //Send bootloader 100 sync chars to get devices to keep bootloader mode..
  for(int i=0;i<100;i++){
    pcaUart.write(0x7F);
    delay(9);
  }
}


void loop() {
  // Forward characters from Hardware Serial to the PCA9685 OE
  if (Serial.available() > 0) {
    char incomingByte = Serial.read();

    pcaUart.write(incomingByte);
  }
}




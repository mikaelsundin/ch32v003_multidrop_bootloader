//Example calling functions stored in booloader from Arduino 

#include <arduino.h>
#include <iap.h>



void setup(){
  uint8_t testdata[64] = {0};

  testdata[0] = 0x41;
  testdata[1] = 0x42;
  testdata[2] = 0x43;


  testdata[61] = 0x41;
  testdata[62] = 0x42;
  testdata[63] = 0x43;


  //Write to last page on CH32V003.
  flash_erase(0x08003FC0);
  flash_write(0x08003FC0, testdata);
}


void loop(){
    
    
    
}


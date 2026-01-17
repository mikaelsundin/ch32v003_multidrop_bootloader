
void setup() {
  //Required to use PA1 and PA2.
  AFIO->PCFR1 &= ~(1 << 15);

  pinMode(PA2, OUTPUT);
  digitalWrite(PA2, HIGH);

}

// the loop function runs over and over again forever
void loop() {
    digitalWrite(PA2, HIGH);
    delay(500);

    //double blink
    digitalWrite(PA2, LOW);
    delayMicroseconds(10);
    digitalWrite(PA2, HIGH);

    delay(100);

    digitalWrite(PA2, LOW); 
    delayMicroseconds(10);
    digitalWrite(PA2, HIGH);
}

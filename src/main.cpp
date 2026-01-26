#include <Arduino.h>
#include "rgb_lcd.h"

rgb_lcd lcd;

void setup() {
  // Initialise la liaison avec le terminal
  Serial.begin(115200);

  // Initialise l'écran LCD
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(112,0,0);

  pinMode(0,INPUT_PULLUP);
  pinMode(2,INPUT_PULLUP);
  pinMode(12,INPUT_PULLUP);

  ledcAttachPin(27,0);
  ledcSetup(0,25000,8);
}

void loop() {
  int bp0 = digitalRead(0);
  int bp1 = digitalRead(2);
  int bp2 = digitalRead(12);
  char pot = analogRead(33);

  Serial.printf("bp0=%d ",bp0);
  Serial.printf("bp1=%d \n",bp1);
  Serial.printf("bp2=%d ",bp2);
  Serial.printf("pot = %d",pot);

  lcd.printf("bp0 = %d ",bp0);
  lcd.printf("bp1 = %d \n",bp1);
  lcd.printf("bp2 = %d ",bp2);
  lcd.printf("pot = %d",pot); 
  
ledcWrite(0,pot/2);


return;
}

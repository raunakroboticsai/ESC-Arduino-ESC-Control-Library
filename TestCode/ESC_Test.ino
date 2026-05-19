
 * ESC_Test.ino
 * 
 * Devlop by Raunak Choudhary
 */
#include <ESC.h>

#define PIN_ESC 9

ESC EDF27(PIN_ESC);
                               //Min - Max
//Change Speed in microseconds: 1000 - 2000

void setup() {
  EDF27.init();
  delay(150); 
}

void loop() {
  EDF27.setSpeed(2000);
  delay(2000);
  EDF27.setSpeed(1500);
  delay(2000);
}

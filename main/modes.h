#include <Arduino.h>


void mode_check() {
  if (MODESWITCH_ENABLED == 1){
  if (UNWRITTEN_MODE_NUMBER == 1) {
    hekate = true; argon = false;
    AMOUNT_OF_PAYLOADS = 3;
    AUTO_INCREASE_PAYLOAD_on = 0;
    }
    else if (UNWRITTEN_MODE_NUMBER == 2) {
    hekate = true; argon = false;
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 0;
    }
    else if (UNWRITTEN_MODE_NUMBER == 3) {
    hekate = false; argon = true;
    AMOUNT_OF_PAYLOADS = 1;
    AUTO_INCREASE_PAYLOAD_on = 0;
    }
    else if (UNWRITTEN_MODE_NUMBER == 4) {
    hekate = true; argon = false;
    AMOUNT_OF_PAYLOADS = 3;
    AUTO_INCREASE_PAYLOAD_on = 1;
    }
    else if (UNWRITTEN_MODE_NUMBER == 5) {
    hekate = true; argon = false;
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 1;
    }
  if (MODES_AVAILABLE == 1) {
    UNWRITTEN_MODE_NUMBER = DEFAULT_MODE;} 
    else if (UNWRITTEN_MODE_NUMBER > MODES_AVAILABLE) {UNWRITTEN_MODE_NUMBER = 1;}
  return;
  }
}

#include <Arduino.h>

extern void confirm_led(int confirmledlength, int colorvalR, int colorvalG, int colorvalB);
extern void setLedColor(const char color[]);
extern void all_leds_off();
extern void cycle_payloads();
extern void cycle_modes();
extern void cycle_other_options();
extern void writetoflash();
extern void pauseVol_payload(int pausetime);
extern void pauseVol_mode(int pausetime);
extern void pauseVol_other_options(int pausetime);
extern void setPayloadColor(int payloadcolornumber);
extern void full_eeprom_reset();
extern void partial_eeprom_reset();
extern void set_dotstar_brightness();
extern void disable_chip();

bool longpress1 = false; bool longpress2 = false; bool longpress3 = false;

void first_long_press() {
  //VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  confirm_led(20, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
  VOL_TICK_TIMER = VOL_TICK_TIMER + (1000);//align to seconds to keep timing
  VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  if (VOLUP_HELD != LOW) {
    VOL_TICK_TIMER = 0;
    longpress1 = false; longpress2 = false; longpress3 = false;
    cycle_payloads();
  } else {
    longpress1 = true;
  }
}

void second_long_press() {
  //VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
  VOL_TICK_TIMER = VOL_TICK_TIMER + (1000);//align to seconds to keep timing
  VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  if (VOLUP_HELD != LOW) {
    VOL_TICK_TIMER = 0;
    longpress1 = false; longpress2 = false; longpress3 = false;
    cycle_modes();
    return;
  } else {
    longpress2 = true;
  }
}

void third_long_press() {
  //VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  confirm_led(20, 0, NEW_DOTSTAR_BRIGHTNESS, 0);
  VOL_TICK_TIMER = VOL_TICK_TIMER + (1000);//align to seconds to keep timing
  VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  if (VOLUP_HELD != LOW) {
    VOL_TICK_TIMER = 0;
    longpress1 = false; longpress2 = false; longpress3 = false;
    cycle_other_options();
  } else {
    longpress3 = true;
  }
}

void fourth_long_press() {
  //VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS);
  VOLUP_HELD = digitalRead(VOLUP_STRAP_PIN);
  if (VOLUP_HELD != LOW) {
    longpress1 = false; longpress2 = false; longpress3 = false;
    VOL_TICK_TIMER = 0;
    confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS);
    full_eeprom_reset();
    return;
  } else {
    setLedColor("red");
    NVIC_SystemReset();
  }
}

void long_press() {
  VOL_TICK_TIMER = 0;
  if (!VOLUP_STRAP_TEST){
  VOLUP_STRAP_TEST = 1;
  EEPROM_VOL_CONTROL_STRAP.write(VOLUP_STRAP_TEST);
  }
  pinMode (VOLUP_STRAP_PIN, INPUT_PULLUP);
  #ifdef ONBOARD_LED
  pinMode (ONBOARD_LED, OUTPUT);
  #endif
  //set initial button values
  bool volup_pressed = false;
  bool volup_released = true;
  bool volup_pressed1 = false;
  bool volup_released1 = true;
  //init timer condition
  while (VOL_TICK_TIMER < MASTER_VOLUP_TIMER) {

    //set to mS
    delayMicroseconds(1000);
    //button conditions
    if (digitalRead(VOLUP_STRAP_PIN == LOW)) {
      volup_pressed = true;
    }
    if (digitalRead(VOLUP_STRAP_PIN != LOW)) {
      volup_pressed = false;
    }
    // increase timer & set indication LED colour
    if (volup_pressed) {
      ++VOL_TICK_TIMER;
      if (VOL_TICK_TIMER < LONG_PRESS_TRIGGER1) {
        setLedColor("blue");
      } else if (VOL_TICK_TIMER > LONG_PRESS_TRIGGER1 && VOL_TICK_TIMER < LONG_PRESS_TRIGGER2) {
        setLedColor("orange");
      } else if (VOL_TICK_TIMER > LONG_PRESS_TRIGGER2 && VOL_TICK_TIMER < LONG_PRESS_TRIGGER3) {
        setLedColor("green");
      } else if (VOL_TICK_TIMER > LONG_PRESS_TRIGGER3 && VOL_TICK_TIMER < FULL_RESET_TRIGGER) {
        setLedColor("white");
      }
    } else if ((!volup_pressed) && VOL_TICK_TIMER > 0) {
      VOL_TICK_TIMER = 0;
      all_leds_off();
      return;
    }
    //final check and execute trigger
    if (VOL_TICK_TIMER == LONG_PRESS_TRIGGER1){
      confirm_led(20, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
      if (digitalRead(VOLUP_STRAP_PIN) != LOW){
        cycle_payloads();
      }
    }
    if (VOL_TICK_TIMER == LONG_PRESS_TRIGGER2){
      confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
      if (digitalRead(VOLUP_STRAP_PIN) != LOW){
        cycle_modes();
      }
    }
    if (VOL_TICK_TIMER == LONG_PRESS_TRIGGER3){
      confirm_led(20, 0, NEW_DOTSTAR_BRIGHTNESS, 0);
      if (digitalRead(VOLUP_STRAP_PIN) != LOW){
        cycle_other_options();
      }
    }
    if (VOL_TICK_TIMER == FULL_RESET_TRIGGER){
      confirm_led(50, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS);
      if (digitalRead(VOLUP_STRAP_PIN) != LOW){
        full_eeprom_reset();
      }
    }
  }
}

void cycle_other_options() {

  for (INDICATED_ITEM = 1; INDICATED_ITEM <= AMOUNT_OF_OTHER_OPTIONS; ++INDICATED_ITEM) {
    for (CURRENT_FLASH = 0; CURRENT_FLASH < (INDICATED_ITEM) ; ++CURRENT_FLASH) {
      setLedColor("black");
#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, LOW);
#endif
      pauseVol_other_options(30);
      setLedColor("green");
#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, HIGH);
#endif
      pauseVol_other_options(10);
    }
    setLedColor("black");
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
#endif
    pauseVol_other_options(100);
  }
  confirm_led(20, 255, NEW_DOTSTAR_BRIGHTNESS, 255);
  VOL_TICK_TIMER = 0;
  return;
}

void pauseVol_other_options(int pausetime) {
#ifdef VOLUP_STRAP_PIN
  pinMode (VOLUP_STRAP_PIN, INPUT_PULLUP);
  i = 0;
  while (i < pausetime) {
    SELECTED = digitalRead(VOLUP_STRAP_PIN);
    if (SELECTED == LOW) {
      if (INDICATED_ITEM == 1) {
        set_dotstar_brightness();
      }
      if (INDICATED_ITEM == 2) {
        partial_eeprom_reset();
      }
      if (INDICATED_ITEM == 3) {
        confirm_led(25, 255, 0, 0);
        confirm_led(25, 0, 0, 255);
        confirm_led(25, 255, 0, 0);
        confirm_led(25, 0, 0, 255);
        disable_chip();
      }
      SELECTED = digitalRead(VOLUP_STRAP_PIN);
      if (SELECTED != LOW) {
        NVIC_SystemReset();
      } else {
        confirm_led(20, 255, NEW_DOTSTAR_BRIGHTNESS, 255);
        delayMicroseconds(1000000);
        return;
      }
    } else {
      delayMicroseconds(10000);
      ++i;
    }
  }
  return;
#endif
}


void cycle_payloads() {

  for (INDICATED_ITEM = 0; INDICATED_ITEM < AMOUNT_OF_PAYLOADS; ++INDICATED_ITEM) {
    for (CURRENT_FLASH = 0; CURRENT_FLASH < (INDICATED_ITEM + 1) ; ++CURRENT_FLASH) {
      setLedColor("black");
#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, LOW);
#endif
      pauseVol_payload(30);
      setPayloadColor(INDICATED_ITEM + 1);
#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, HIGH);
#endif
      pauseVol_payload(10);
    }
    setLedColor("black");
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
#endif
    pauseVol_payload(100);
  }
  confirm_led(20, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
  VOL_TICK_TIMER = 0;
  return;
}

void pauseVol_payload(int pausetime) {
#ifdef VOLUP_STRAP_PIN
  pinMode (VOLUP_STRAP_PIN, INPUT_PULLUP);
  i = 0;
  while (i < pausetime) {
    SELECTED = digitalRead(VOLUP_STRAP_PIN);
    if (SELECTED == LOW) {
      UNWRITTEN_PAYLOAD_NUMBER = INDICATED_ITEM + 1;
      writetoflash();
      confirm_led(20, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
      SELECTED = digitalRead(VOLUP_STRAP_PIN);
      if (SELECTED != LOW) {
        NVIC_SystemReset();
      } else {
        confirm_led(20, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
        delayMicroseconds(1000000);
        return;
      }
    } else {
      delayMicroseconds(10000);
      ++i;
    }
  }
  return;
#endif
}

void cycle_modes() {
  if (MODES_AVAILABLE != 1) {
    for (INDICATED_ITEM = 0; INDICATED_ITEM < MODES_AVAILABLE; ++INDICATED_ITEM) {
      for (CURRENT_FLASH = 0; CURRENT_FLASH < (INDICATED_ITEM + 1) ; ++CURRENT_FLASH) {
        setLedColor("black");
#ifdef ONBOARD_LED
        digitalWrite(ONBOARD_LED, LOW);
#endif
        pauseVol_mode(30);
        setLedColor("orange");
#ifdef ONBOARD_LED
        digitalWrite(ONBOARD_LED, HIGH);
#endif
        pauseVol_mode(10);
      }
      setLedColor("black");
#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, LOW);
#endif
      pauseVol_mode(100);
    }
    confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
    VOL_TICK_TIMER = 0;
    return;
  } else {
    VOL_TICK_TIMER = 0;
    return;
  }
}

void pauseVol_mode(int pausetime) {
#ifdef VOLUP_STRAP_PIN
  pinMode (VOLUP_STRAP_PIN, INPUT_PULLUP);
  i = 0;
  while (i < pausetime) {
    SELECTED = digitalRead(VOLUP_STRAP_PIN);
    if (SELECTED == LOW) {
      UNWRITTEN_MODE_NUMBER = INDICATED_ITEM + 1;
      writetoflash();
      confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
      NVIC_SystemReset();
    } else {
      delayMicroseconds(10000);
      ++i;
    }
  }
  return;
#endif
}

void set_dotstar_brightness() {
#ifdef DOTSTAR_ENABLED
  delayMicroseconds(1000000);
  pinMode (VOLUP_STRAP_PIN, INPUT_PULLUP);
  int currentfade = 0; int currentbrightness = 0;
  while (currentfade < 3) {
    for (i = 0; i < 256; ++i) {
      SELECTED = digitalRead(VOLUP_STRAP_PIN);
      if (SELECTED == LOW) {
        EEPROM_DOTSTAR_BRIGHTNESS.write(i);
        confirm_led(20, i, i, i);
        NVIC_SystemReset();
      }
      strip.setPixelColor(0, i, i, i);
      strip.show();
      delayMicroseconds(10000);
    }
    for (i = 255; i > 1; --i) {
      SELECTED = digitalRead(VOLUP_STRAP_PIN);
      if (SELECTED == LOW) {
        EEPROM_DOTSTAR_BRIGHTNESS.write(i);
        confirm_led(20, i, i, i);
        NVIC_SystemReset();
      }
      strip.setPixelColor(0, i, i, i);
      strip.show();
      delayMicroseconds(10000);
    }
    ++currentfade;
  }
  NVIC_SystemReset();
#endif
}

void partial_eeprom_reset() {
  UNWRITTEN_MODE_NUMBER = DEFAULT_MODE;
  UNWRITTEN_PAYLOAD_NUMBER = 1;
  writetoflash();
  all_leds_off();
  NVIC_SystemReset();
}

void full_eeprom_reset() {
  EEPROM_INITIAL_WRITE = !EEPROM_INITIAL_WRITE;
  UNWRITTEN_MODE_NUMBER = !DEFAULT_MODE;
  UNWRITTEN_PAYLOAD_NUMBER = !UNWRITTEN_PAYLOAD_NUMBER;
  USB_STRAP_TEST = !USB_STRAP_TEST;
  VOLUP_STRAP_TEST = !VOLUP_STRAP_TEST;
  JOYCON_STRAP_TEST = !JOYCON_STRAP_TEST;
  EEPROM_EMPTY.write(EEPROM_INITIAL_WRITE);
  EEPROM_USB_REBOOT_STRAP.write(USB_STRAP_TEST);
  EEPROM_VOL_CONTROL_STRAP.write(VOLUP_STRAP_TEST);
  EEPROM_JOYCON_CONTROL_STRAP.write(JOYCON_STRAP_TEST);
  EEPROM_DOTSTAR_BRIGHTNESS.write(DEFAULT_DOTSTAR_BRIGHTNESS);
  EEPROM_CHIP_DISABLED.write(false);
  writetoflash();
  all_leds_off();
  NVIC_SystemReset();
}

void disable_chip() {
  EEPROM_CHIP_DISABLED.write(true);
  NVIC_SystemReset();
}

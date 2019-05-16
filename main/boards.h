#include <Arduino.h>

//set input/output pin numbers
#ifdef TRINKET
#define BOARDNAME TRINKET_BOARD
#define MODCHIP
#define DOTSTAR_ENABLED 1
#define PAYLOAD_INCREASE_PIN 1     // Payload increase pin - touch to ground by default.
#define USB_LOW_RESET 2
#define MODE_CHANGE_PIN 4       // 
#define JOYCON_STRAP_PIN 3         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 0        
#define ONBOARD_LED 13
#endif

#ifdef TRINKETMETHOD3
#define BOARDNAME TRINKET_BOARD
#define MODCHIP
#define DOTSTAR_ENABLED 1
#define PAYLOAD_INCREASE_PIN 1     // Payload increase pin - touch to ground by default.
#define USB_LOW_RESET 4
//#define MODE_CHANGE_PIN 4       // 
#define WAKEUP_PIN_RISING 2        // Method 3 pin to M92T36 pin 5 capacitor / rail
#define JOYCON_STRAP_PIN 3         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 0         
#define ONBOARD_LED 13
#endif

#ifdef TRINKETLEGACY3
#define BOARDNAME TRINKET_BOARD
#define MODCHIP
#define DOTSTAR_ENABLED 1
#define PAYLOAD_INCREASE_PIN 1     // Payload increase pin - touch to ground by default.
#define USB_LOW_RESET 2
//#define MODE_CHANGE_PIN 2       // 
#define WAKEUP_PIN_RISING 4        // Method 3 pin to M92T36 pin 5 capacitor / rail
#define JOYCON_STRAP_PIN 3         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 0          
#define ONBOARD_LED 13
#endif

#ifdef REBUG
#define BOARDNAME REBUG_BOARD
#define MODCHIP
#define DOTSTAR_ENABLED 1
#define PAYLOAD_INCREASE_PIN 1     // Payload increase pin - touch to ground by default.
#define USB_LOW_RESET 2
#define MODE_CHANGE_PIN 4       // 
#define JOYCON_STRAP_PIN 3         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 0        
#define ONBOARD_LED 13
#endif

#ifdef GEMMA
#define BOARDNAME GEMMA_BOARD
#define MODCHIP
#define DOTSTAR_ENABLED 1
#define USB_LOW_RESET 1
//#define PAYLOAD_INCREASE_PIN 1     // Payload increase pin - touch to ground by default.
#define MODE_CHANGE_PIN 4       //       
#define JOYCON_STRAP_PIN 2         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 0 
#define ONBOARD_LED 13
#endif

#ifdef ITSYBITSY
#define BOARDNAME ITSYBITSY_BOARD
#define MODCHIP
#define DOTSTAR_ENABLED 1
#define USB_LOW_RESET 7
#define PAYLOAD_INCREASE_PIN 10     // Payload increase pin - touch to ground by default.
#define MODE_CHANGE_PIN 12       //       
#define JOYCON_STRAP_PIN 9         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 11 
#define INTERNAL_DS_DATA 41
#define INTERNAL_DS_CLK 40
#define ONBOARD_LED 13
#endif

#ifdef FEATHER
#define BOARDNAME FEATHER_BOARD
#define MODCHIP
#define DOTSTAR_ENABLED 1
#define USB_LOW_RESET 6
#define PAYLOAD_INCREASE_PIN 10     // Payload increase pin - touch to ground by default.
#define MODE_CHANGE_PIN 12        
#define JOYCON_STRAP_PIN 9         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 11  
#define INTERNAL_DS_DATA 41
#define INTERNAL_DS_CLK 40
#define ONBOARD_LED 13
#endif

#ifdef RCMX86_INTERNAL
#define BOARDNAME RCMX86_BOARD
#define MODCHIP
#define USB_LOW_RESET 4
#define JOYCON_STRAP_PIN 2         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN 1        
#define USB_LOGIC 3
#endif

#ifdef EXEN_MINI
#define MODCHIP
//#define DOTSTAR_ENABLED 1
#define USB_LOW_RESET A2
#define PAYLOAD_INCREASE_PIN A1     // Payload increase pin - touch to ground by default.
//#define MODE_CHANGE_PIN 11       //       
#define JOYCON_STRAP_PIN A3         // Solder to pin 10 on joycon rail
#define VOLUP_STRAP_PIN A4
#define ONBOARD_LED 8
#endif

#ifdef RCMX86
#define BOARDNAME RCMX86DONGLE_BOARD
#define DONGLE
#define LOOK_FOR_TEGRA_SECONDS 10 //How long to look for Tegra for & flash LED in search phase. Time in seconds
#define RESET_INSTEAD_OF_SLEEP 1 //Instead of sleeping after look for Tegra timeout, device will reset. This will loop until Tegra is found. Affects autoincrease. 1 = On, 0 = Off
#define ONBOARD_LED 4
#define USBCC_PIN 2
#define USB_VCC_PIN 0
#define DCDC_EN_PIN 3
#define VOLUP_STRAP_PIN 5
#endif

#ifdef R4S
#define DONGLE
#define ONBOARD_LED 17
#define VOLUP_STRAP_PIN 18
#endif

#ifdef GENERIC_TRINKET_DONGLE
#define BOARDNAME GENERICTRINKET_BOARD
#define DONGLE
#define DOTSTAR_ENABLED 1
#define PAYLOAD_INCREASE_PIN 1     // Payload increase pin - touch to ground by default.
#define MODE_CHANGE_PIN 4
#define ONBOARD_LED 13
#define VOLUP_STRAP_PIN 2
#endif

#ifdef GENERIC_GEMMA_DONGLE
#define BOARDNAME GENERICGEMMA_BOARD
#define DONGLE
#define DOTSTAR_ENABLED 1
#define PAYLOAD_INCREASE_PIN 0     // Payload increase pin - touch to ground by default.
#define MODE_CHANGE_PIN 1
#define ONBOARD_LED 13
#define VOLUP_STRAP_PIN 2
#endif

#ifdef DRAGONINJECTOR
#define BOARDNAME DRAGON_INJECTOR_BOARD
#define DONGLE
#define BATTERY_LEVEL_CHECK A0
#define VOLUP_STRAP_PIN 0
#define REDLED 7
#define GREENLED 8
#define BLUELED 2
#define GOODBATT 955
#define FLATBATT 880
#define MODE_CHANGE_PIN 4
//#define ONBOARD_LED 13
#endif

#ifdef MODCHIP
#define LOOK_FOR_TEGRA_SECONDS 2 //How long to look for Tegra for & flash LED in search phase. Time in seconds
#define RESET_INSTEAD_OF_SLEEP 0 //Instead of sleeping after look for Tegra timeout, device will reset. This will loop until Tegra is found. Affects autoincrease. 1 = On, 0 = Off
#define DISABLE_USB 1
#endif

#ifdef DONGLE
#define LOOK_FOR_TEGRA_SECONDS 2 //How long to look for Tegra for & flash LED in search phase. Time in seconds
#define RESET_INSTEAD_OF_SLEEP 0 //Instead of sleeping after look for Tegra timeout, device will reset. This will loop until Tegra is found. Affects autoincrease. 1 = On, 0 = Off
#define DISABLE_USB 0
#endif

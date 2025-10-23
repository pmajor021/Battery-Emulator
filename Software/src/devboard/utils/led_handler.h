#ifndef LED_H_
#define LED_H_

#include "../../devboard/utils/types.h"
#include "../../lib/adafruit-Adafruit_NeoPixel/Adafruit_NeoPixel.h"
#include "../../lib/gavinlyonsrepo-TM1638plus/src/Max7219plus.h"

class LED {
 public:
  LED(gpio_num_t pin, uint8_t maxBrightness)
      : pixels(pin), max_brightness(maxBrightness), brightness(maxBrightness), mode(led_mode_enum::CLASSIC) {}

  LED(led_mode_enum mode, gpio_num_t pin, uint8_t maxBrightness)
      : pixels(pin), max_brightness(maxBrightness), brightness(maxBrightness), mode(mode) {}

  void exe(void);

 private:
  Adafruit_NeoPixel pixels;
  uint8_t max_brightness;
  uint8_t brightness;
  led_mode_enum mode;

  void classic_run(void);
  void flow_run(void);
  void heartbeat_run(void);

  uint8_t up_down(float middle_point_f);
  int LED_PERIOD_MS = 3000;
};

bool led_init(void);
#ifdef __HW_LILYGO2CAN_H__
bool tm1637_init(void);
#endif
void led_exe(void);

#endif  // LED_H_

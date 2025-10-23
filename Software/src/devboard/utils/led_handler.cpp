#include "led_handler.h"
#include "../../datalayer/datalayer.h"
#include "../../devboard/hal/hal.h"
#include "events.h"
#include "value_mapping.h"
#include "../../devboard/webserver/webserver.h"
#include "../../battery/BATTERIES.h"

#define COLOR_GREEN(x) (((uint32_t)0 << 16) | ((uint32_t)x << 8) | 0)
#define COLOR_YELLOW(x) (((uint32_t)x << 16) | ((uint32_t)x << 8) | 0)
#define COLOR_RED(x) (((uint32_t)x << 16) | ((uint32_t)0 << 8) | 0)
#define COLOR_BLUE(x) (((uint32_t)0 << 16) | ((uint32_t)0 << 8) | x)

#define BPM_TO_MS(x) ((uint16_t)((60.0f / ((float)x)) * 1000))

static const float heartbeat_base = 0.15;
static const float heartbeat_peak1 = 0.80;
static const float heartbeat_peak2 = 0.55;
static const float heartbeat_deviation = 0.05;

static LED* led;

// MAX7219 Display
static uint8_t SDIN_PIN = 16; 
static uint8_t SCLK_PIN = 45; 
static uint8_t CS_PIN   = 15; 
static uint8_t totalDisplays = 1; 
static uint16_t CommDelay = 0;
MAX7219plus_Model6 MAX7219(CS_PIN, SCLK_PIN,SDIN_PIN, CommDelay, totalDisplays);

static unsigned long lastPrintTime = 0;
static const unsigned long interval = 1000;
static int battery_show = 1;

bool led_init(void) {
  if (!esp32hal->alloc_pins("LED", esp32hal->LED_PIN())) {
    DEBUG_PRINTF("LED setup failed\n");
    return false;
  }

  led = new LED(datalayer.battery.status.led_mode, esp32hal->LED_PIN(), esp32hal->LED_MAX_BRIGHTNESS());

  MAX7219.InitDisplay(MAX7219.ScanEightDigit, MAX7219.DecodeModeNone);
	MAX7219.ClearDisplay();
  char bestring[8];
  sprintf(bestring, "%s", version_number);
  MAX7219.DisplayText(bestring, MAX7219.AlignLeft);

  return true;
}

void led_exe(void) {
  led->exe();
}

void LED::exe(void) {
  //update MAX7219 display if connected
  static bool max7219_connected = (esp32hal->MAX7219_CLK_PIN() != GPIO_NUM_NC);
  if (max7219_connected) {
  unsigned long currentTime = millis();
  if (currentTime - lastPrintTime >= interval) {
    lastPrintTime = currentTime;
    switch ((millis() /1000u) % 16) {
      case 0 ... 1: {
          char maxstring[8];
          sprintf(maxstring, "SOC   ");
          MAX7219.DisplayText(maxstring, MAX7219.AlignLeft);
        }
        break;
      case 2 ... 3: {
        char reported_soc [8];
        sprintf(reported_soc, "%.2f", ((float)datalayer.battery.status.reported_soc / 100.0f));
        MAX7219.DisplayText(reported_soc, MAX7219.AlignLeft); 
        }
        break;
      case 4 ... 5: {
          char maxstring[8];
          sprintf(maxstring, "VOLT  ");
          MAX7219.DisplayText(maxstring, MAX7219.AlignLeft);
        }
        break;
      case 6 ... 7: {
        char reported_voltage [8];
        sprintf(reported_voltage, "%.1fV", ((float)datalayer.battery.status.voltage_dV / 10.0f));
        MAX7219.DisplayText(reported_voltage,MAX7219.AlignLeft); 
        }
        break;
      case 8 ... 9: {
          char maxstring[8];
          sprintf(maxstring, "CURR  ");
          MAX7219.DisplayText(maxstring, MAX7219.AlignLeft);
        }
        break;
      case 10 ... 11: {
        char reported_current [8];
        sprintf(reported_current, "%.1fA ", ((float)datalayer.battery.status.current_dA / 10.0f));
        MAX7219.DisplayText(reported_current, MAX7219.AlignLeft);
        }
        break;
      case 12 ... 13:{
          char maxstring[8];
          sprintf(maxstring, "CAP   ");
          MAX7219.DisplayText(maxstring, MAX7219.AlignLeft);
        }
        break;
      case 14 ...15: {
        char reported_capacity [8];
        sprintf(reported_capacity, "%.2fk", ((float)datalayer.battery.status.remaining_capacity_Wh / 1000.0f));
        MAX7219.DisplayText(reported_capacity, MAX7219.AlignLeft);   
        }
        break;
    }
    }
  }
    
  // Update brightness
  switch (datalayer.battery.status.led_mode) {
    case led_mode_enum::FLOW:
      flow_run();
      break;
    case led_mode_enum::HEARTBEAT:
      heartbeat_run();
      break;
    case led_mode_enum::CLASSIC:
    default:
      classic_run();
      break;
  }

  // Set color
  switch (get_emulator_status()) {
    case EMULATOR_STATUS::STATUS_OK:
      pixels.setPixelColor(COLOR_GREEN(brightness));  // Green pulsing LED
      break;
    case EMULATOR_STATUS::STATUS_WARNING:
      pixels.setPixelColor(COLOR_YELLOW(brightness));  // Yellow pulsing LED
      break;
    case EMULATOR_STATUS::STATUS_ERROR:
      pixels.setPixelColor(COLOR_RED(esp32hal->LED_MAX_BRIGHTNESS()));  // Red LED full brightness
      break;
    case EMULATOR_STATUS::STATUS_UPDATING:
      pixels.setPixelColor(COLOR_BLUE(brightness));  // Blue pulsing LED
      break;
  }

  pixels.show();  // This sends the updated pixel color to the hardware.
}

void LED::classic_run(void) {
  // Determine how bright the LED should be
  brightness = up_down(0.5);
}

void LED::flow_run(void) {
  // Determine how bright the LED should be
  if (datalayer.battery.status.active_power_W < -50) {
    // Discharging
    brightness = max_brightness - up_down(0.95);
  } else if (datalayer.battery.status.active_power_W > 50) {
    // Charging
    brightness = up_down(0.95);
  } else {
    brightness = up_down(0.5);
  }
}

void LED::heartbeat_run(void) {
  uint16_t period;
  switch (get_event_level()) {
    case EVENT_LEVEL_WARNING:
      // phew, starting to sweat here
      period = BPM_TO_MS(70);
      break;
    case EVENT_LEVEL_ERROR:
      // omg omg OMG OMGG
      period = BPM_TO_MS(100);
      break;
    default:
      // Keep default chill bpm (ba-dunk... ba-dunk... ba-dunk...)
      period = BPM_TO_MS(35);
      break;
  }

  uint16_t ms = (uint16_t)(millis() % period);

  float period_pct = ((float)ms) / period;
  float brightness_f;

  if (period_pct < 0.10) {
    brightness_f = map_float(period_pct, 0.00f, 0.10f, heartbeat_base, heartbeat_base - heartbeat_deviation);
  } else if (period_pct < 0.20) {
    brightness_f = map_float(period_pct, 0.10f, 0.20f, heartbeat_base - heartbeat_deviation,
                             heartbeat_base - heartbeat_deviation * 2);
  } else if (period_pct < 0.25) {
    brightness_f = map_float(period_pct, 0.20f, 0.25f, heartbeat_base - heartbeat_deviation * 2, heartbeat_peak1);
  } else if (period_pct < 0.30) {
    brightness_f = map_float(period_pct, 0.25f, 0.30f, heartbeat_peak1, heartbeat_base - heartbeat_deviation);
  } else if (period_pct < 0.40) {
    brightness_f = map_float(period_pct, 0.30f, 0.40f, heartbeat_base - heartbeat_deviation, heartbeat_peak2);
  } else if (period_pct < 0.55) {
    brightness_f = map_float(period_pct, 0.40f, 0.55f, heartbeat_peak2, heartbeat_base + heartbeat_deviation * 2);
  } else {
    brightness_f = map_float(period_pct, 0.55f, 1.00f, heartbeat_base + heartbeat_deviation * 2, heartbeat_base);
  }

  brightness = (uint8_t)(brightness_f * esp32hal->LED_MAX_BRIGHTNESS());
}

uint8_t LED::up_down(float middle_point_f) {
  // Determine how bright the LED should be
  middle_point_f = CONSTRAIN(middle_point_f, 0.0f, 1.0f);
  uint16_t middle_point = (uint16_t)(middle_point_f * LED_PERIOD_MS);
  uint16_t ms = (uint16_t)(millis() % LED_PERIOD_MS);
  brightness = map_uint16(ms, 0, middle_point, 0, max_brightness);
  if (ms < middle_point) {
    brightness = map_uint16(ms, 0, middle_point, 0, max_brightness);
  } else {
    brightness = esp32hal->LED_MAX_BRIGHTNESS() - map_uint16(ms, middle_point, LED_PERIOD_MS, 0, max_brightness);
  }
  return CONSTRAIN(brightness, 0, max_brightness);
}

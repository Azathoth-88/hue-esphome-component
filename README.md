# esphome-components
Custom (light) components for ESPHome


## RGB-CT Light
Create a real Philips HUE Lightstrip look-a-like with ESPHome and RGB+CT lightstrips.

- Allows mixing rgb leds for color temperature outside of range of the white leds.
- Same behaviour as native Hue lights in HA: RGB and white combiend into single light.
- Can be 100% color matched with other HUE lights.
- Also works with light bulbs

Color temperatures between `warm_white_color_temperature` and `cold_white_color_temperature` use only the
white LEDs. Beyond them the white LED cross-fades into the RGB blackbody color, reaching pure RGB at
`max_warm_color_temperature` / `max_cold_color_temperature`.
Brightness follows the light's `gamma_correct` setting (ESPHome default 2.8).

### Installation:
1. Add the component as an external component:
   ```
   external_components:
     - source: github://Azathoth-88/hue-esphome-component
       components: [rgbcct]
   ```
   Or copy the `components` folder into the esphome config directory (e.g. /config/esphome) and use
   `source: {type: local, path: components}`.
2. Use configuration from below.

**Example ESpHome config:**

```
light:
  - platform: rgbcct
    name: "${device_name}: RGBCCT Light"
    red: pwm_r
    green: pwm_g
    blue: pwm_b
    warm_white: pwm_ww
    cold_white: pwm_cw
    cold_white_color_temperature: 6500 K
    warm_white_color_temperature: 2400 K
    # In Kelvin or Mireds
    max_warm_color_temperature: 500 mireds  # Optional defaults to 500 mireds
    max_cold_color_temperature: 153 mireds  # Optional defaults to 153 mireds
    constant_brightness: false  # Optional defaults to false, make white brightness levels combine to max_combined_white_level
    max_combined_white_level: 1  # Optional defaults to 1, reduce if light is unable to support max brightness

output:
  - platform: esp8266_pwm
    pin: GPIO13
    id: pwm_ww

  - platform: esp8266_pwm
    pin: GPIO12
    id: pwm_g
    max_power: 45%

  - platform: esp8266_pwm
    pin: GPIO14
    id: pwm_b
    max_power: 30%

  - platform: esp8266_pwm
    pin: GPIO5
    id: pwm_cw
    
  - platform: esp8266_pwm
    pin: GPIO4
    id: pwm_r
    
```

In the example above replace the correct values for the GPIO pins.
Adjust max_power setting for each channel to match your lightstrip.
Above settings were tested using a (very common) 24V RGB-CT 5050 ledstrip from Amazon/Ali.
Adjust the levels until the light exactly matches.

Adjust min/max color temperature of light as desired.

### LocalBytes Smart Bulb (9W RGB+CT)
ESP8285 with all five channels on plain PWM (no LED driver chip). The white LEDs are 3000K and 6000K.
The bulb's power supply is sized for RGB *or* white, so use `constant_brightness: true`.

```
esp8266:
  board: esp8285

light:
  - platform: rgbcct
    name: "${device_name}: RGBCCT Light"
    red: pwm_r
    green: pwm_g
    blue: pwm_b
    warm_white: pwm_ww
    cold_white: pwm_cw
    cold_white_color_temperature: 6000 K
    warm_white_color_temperature: 3000 K
    max_warm_color_temperature: 2000 K
    max_cold_color_temperature: 6500 K
    constant_brightness: true

output:
  - {platform: esp8266_pwm, pin: GPIO4,  id: pwm_r,  min_power: 0.0005, zero_means_zero: true}
  - {platform: esp8266_pwm, pin: GPIO12, id: pwm_g,  min_power: 0.0005, zero_means_zero: true}
  - {platform: esp8266_pwm, pin: GPIO14, id: pwm_b,  min_power: 0.0005, zero_means_zero: true}
  - {platform: esp8266_pwm, pin: GPIO5,  id: pwm_cw, min_power: 0.0005, zero_means_zero: true}
  - {platform: esp8266_pwm, pin: GPIO13, id: pwm_ww, min_power: 0.0005, zero_means_zero: true}
```

Tune `max_power` on the RGB outputs until the RGB mix around 3000K matches the warm white LED.

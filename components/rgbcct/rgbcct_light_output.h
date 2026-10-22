#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace rgbcct {

class RGBCCTLightOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { this->red_ = red; }
  void set_green(output::FloatOutput *green) { this->green_ = green; }
  void set_blue(output::FloatOutput *blue) { this->blue_ = blue; }
  void set_cold_white(output::FloatOutput *cold_white) { this->cold_white_ = cold_white; }
  void set_warm_white(output::FloatOutput *warm_white) { this->warm_white_ = warm_white; }
  void set_constant_brightness(bool constant_brightness) { this->constant_brightness_ = constant_brightness; }
  void set_cold_white_temperature(float cold_white_temperature) {
    this->cold_white_mireds_ = cold_white_temperature;
  }
  void set_warm_white_temperature(float warm_white_temperature) {
    this->warm_white_mireds_ = warm_white_temperature;
  }
  void set_max_warm_color_temperature(float warm_color_temperature) {
    this->max_mireds_ = warm_color_temperature;
  }
  void set_max_cold_color_temperature(float cold_color_temperature) {
    this->min_mireds_ = cold_color_temperature;
  }
  void set_max_combined_white_level(float max_combined_white_level) {
    this->max_combined_white_level_ = max_combined_white_level;
  }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(this->min_mireds_);
    traits.set_max_mireds(this->max_mireds_);
    return traits;
  }

  void write_state(light::LightState *state) override {
    float cwhite = 0.0f, wwhite = 0.0f;
    float red = state->current_values.get_red();
    float green = state->current_values.get_green();
    float blue = state->current_values.get_blue();
    float color_temp_mired = state->current_values.get_color_temperature();
    // Apply the light's gamma_correct curve to brightness so dimming is perceptually linear
    const float brightness =
        state->gamma_correct_lut(state->current_values.get_brightness() * state->current_values.get_state());
    const bool is_ct_mode = state->current_values.get_color_mode() == light::ColorMode::COLOR_TEMPERATURE;

    if (is_ct_mode) {
      // Charity blackbody LUT — 2° CMF, sRGB, 1000–6500K every 100K
      // http://www.vendian.org/mncharity/dir3/blackbody/
      float kelvin = 1000000.0f / color_temp_mired;
      // Clamp to LUT range
      if (kelvin < 1000.0f) kelvin = 1000.0f;
      if (kelvin > 6500.0f) kelvin = 6500.0f;
      // Index: 1000K = 0, 1100K = 1, ..., 6500K = 55
      int idx = (int)((kelvin - 1000.0f) / 100.0f);
      if (idx >= LUT_SIZE - 1) idx = LUT_SIZE - 2;
      float frac = (kelvin - (1000.0f + idx * 100.0f)) / 100.0f;
      if (frac < 0.0f) frac = 0.0f;
      if (frac > 1.0f) frac = 1.0f;

      red   = r_lut[idx] + (r_lut[idx + 1] - r_lut[idx]) * frac;
      green = g_lut[idx] + (g_lut[idx + 1] - g_lut[idx]) * frac;
      blue  = b_lut[idx] + (b_lut[idx + 1] - b_lut[idx]) * frac;

      // CW/WW blend based on where CT falls relative to white LED limits
      const float ww_fraction = (color_temp_mired - this->cold_white_mireds_) /
                                (this->warm_white_mireds_ - this->cold_white_mireds_);
      const float cw_fraction = 1.0f - ww_fraction;
      const float max_cw_ww = std::max(ww_fraction, cw_fraction);
      cwhite = (cw_fraction / max_cw_ww);
      wwhite = (ww_fraction / max_cw_ww);

      // RGB only outside the white LED range: cross-fade from white to RGB between the
      // white LED limit and the configured min/max color temperature
      float rgb_level = 0.0f;
      if (color_temp_mired < this->cold_white_mireds_) {
        rgb_level = fade_level_(this->cold_white_mireds_ - color_temp_mired, this->cold_white_mireds_ - this->min_mireds_);
        cwhite = 1.0f - rgb_level;
      } else if (color_temp_mired > this->warm_white_mireds_) {
        rgb_level = fade_level_(color_temp_mired - this->warm_white_mireds_, this->max_mireds_ - this->warm_white_mireds_);
        wwhite = 1.0f - rgb_level;
      }
      red *= rgb_level;
      green *= rgb_level;
      blue *= rgb_level;
    }

    // sRGB -> linear for RGB mode only; the blackbody LUT values are already linear
    if (!is_ct_mode) {
      red   = (red   > 0.04045f) ? powf((red   + 0.055f) / 1.055f, 2.4f) : (red   / 12.92f);
      green = (green > 0.04045f) ? powf((green + 0.055f) / 1.055f, 2.4f) : (green / 12.92f);
      blue  = (blue  > 0.04045f) ? powf((blue  + 0.055f) / 1.055f, 2.4f) : (blue  / 12.92f);
    }

    red *= brightness;
    green *= brightness;
    blue *= brightness;

    if (!constant_brightness_) {
      cwhite *= brightness;
      wwhite *= brightness;
    } else {
      wwhite = clamp<float>(wwhite, 0.0f, 1.0f);
      cwhite = clamp<float>(cwhite, 0.0f, 1.0f);
      const float sum = (cwhite > 0.0f || wwhite > 0.0f) ? (cwhite + wwhite) : 1.0f;
      const float white_level = std::min(this->max_combined_white_level_, std::max(cwhite, wwhite));
      cwhite = brightness * white_level * (cwhite / sum);
      wwhite = brightness * white_level * (wwhite / sum);
    }

    red    = clamp<float>(red,    0.0f, 1.0f);
    green  = clamp<float>(green,  0.0f, 1.0f);
    blue   = clamp<float>(blue,   0.0f, 1.0f);
    wwhite = clamp<float>(wwhite, 0.0f, 1.0f);
    cwhite = clamp<float>(cwhite, 0.0f, 1.0f);

    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
    this->warm_white_->set_level(wwhite);
    this->cold_white_->set_level(cwhite);
  }

 protected:
  // Fraction of the way through a fade of the given span; a zero/negative span switches instantly
  static float fade_level_(float distance, float span) {
    return span > 0.0f ? clamp<float>(distance / span, 0.0f, 1.0f) : 1.0f;
  }

  output::FloatOutput *red_{nullptr};
  output::FloatOutput *green_{nullptr};
  output::FloatOutput *blue_{nullptr};
  output::FloatOutput *cold_white_{nullptr};
  output::FloatOutput *warm_white_{nullptr};
  bool constant_brightness_{false};
  float cold_white_mireds_{0.0f};
  float warm_white_mireds_{0.0f};
  float min_mireds_{0.0f};
  float max_mireds_{0.0f};
  float max_combined_white_level_{1.0f};

  // Charity blackbody LUT — 2° CMF, sRGB primaries, 1000–6500K every 100K
  // Linear (not gamma-encoded) R G B float columns of bbr_color.txt
  // http://www.vendian.org/mncharity/dir3/blackbody/
  static constexpr int LUT_SIZE = 56;
  static constexpr float k_lut[LUT_SIZE] PROGMEM = {
    1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900,
    2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900,
    3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900,
    4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900,
    5000, 5100, 5200, 5300, 5400, 5500, 5600, 5700, 5800, 5900,
    6000, 6100, 6200, 6300, 6400, 6500
  };
  static constexpr float r_lut[LUT_SIZE] PROGMEM = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 1000-1900K
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 2000-2900K
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 3000-3900K
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 4000-4900K
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 5000-5900K
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f                                 // 6000-6500K
  };
  static constexpr float g_lut[LUT_SIZE] PROGMEM = {
    0.0337f, 0.0592f, 0.0846f, 0.1096f, 0.1341f, 0.1578f, 0.1806f, 0.2025f, 0.2235f, 0.2434f,
    0.2647f, 0.2889f, 0.3126f, 0.3360f, 0.3589f, 0.3814f, 0.4034f, 0.4250f, 0.4461f, 0.4668f,
    0.4870f, 0.5067f, 0.5259f, 0.5447f, 0.5630f, 0.5809f, 0.5983f, 0.6153f, 0.6318f, 0.6480f,
    0.6636f, 0.6789f, 0.6938f, 0.7083f, 0.7223f, 0.7360f, 0.7494f, 0.7623f, 0.7750f, 0.7872f,
    0.7992f, 0.8108f, 0.8221f, 0.8330f, 0.8437f, 0.8541f, 0.8642f, 0.8740f, 0.8836f, 0.8929f,
    0.9019f, 0.9107f, 0.9193f, 0.9276f, 0.9357f, 0.9436f
  };
  static constexpr float b_lut[LUT_SIZE] PROGMEM = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0033f, 0.0120f, 0.0219f, 0.0331f, 0.0454f, 0.0588f, 0.0734f, 0.0889f, 0.1054f, 0.1229f,
    0.1411f, 0.1602f, 0.1800f, 0.2005f, 0.2216f, 0.2433f, 0.2655f, 0.2881f, 0.3112f, 0.3346f,
    0.3583f, 0.3823f, 0.4066f, 0.4310f, 0.4556f, 0.4803f, 0.5051f, 0.5299f, 0.5548f, 0.5797f,
    0.6045f, 0.6293f, 0.6541f, 0.6787f, 0.7032f, 0.7277f, 0.7519f, 0.7760f, 0.8000f, 0.8238f,
    0.8473f, 0.8707f, 0.8939f, 0.9168f, 0.9396f, 0.9621f
  };
};

}  // namespace rgbcct
}  // namespace esphome

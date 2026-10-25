#pragma once

// Detail page of one ventilation unit: device registry, the 24 h history
// fetched from the unit's own web dashboard (GET http://<ip>/history, see
// VentoSync components/ventilation_logic/history.h) and the LVGL chart.

#include <array>
#include <cstdlib>
#include <string>
#include <vector>
#include "esphome.h"
#include "dashboard.h"

namespace dash {

// ---------------------------------------------------------------------------
// Device registry (filled by packages/ventosync_device.yaml at boot)
// ---------------------------------------------------------------------------
struct Device {
  const char *title = "";
  esphome::text_sensor::TextSensor *fan = nullptr;
  esphome::text_sensor::TextSensor *direction = nullptr;
  esphome::text_sensor::TextSensor *version = nullptr;
  esphome::text_sensor::TextSensor *config = nullptr;
  esphome::text_sensor::TextSensor *ip = nullptr;
  esphome::sensor::Sensor *level = nullptr;
  esphome::sensor::Sensor *rpm = nullptr;
  esphome::sensor::Sensor *filter_days = nullptr;
  esphome::sensor::Sensor *wifi = nullptr;
  esphome::binary_sensor::BinarySensor *filter_alarm = nullptr;
};

static const int MAX_DEVICES = 6;
inline std::array<Device, MAX_DEVICES> devices;
inline int selected = 0;         // device shown on page_detail
inline bool detail_open = false;  // page_detail is on screen (history auto refresh)

inline Device &current() { return devices[selected]; }

// "Floor: 1 | Room: 1 | Device: 3 | Phase: A" -> "Gerät 3 · Phase A"
inline std::string short_config(const std::string &cfg) {
  const size_t dev = cfg.find("Device: ");
  const size_t phase = cfg.find("Phase: ");
  if (dev == std::string::npos || phase == std::string::npos)
    return text_or(cfg);
  const size_t dev_end = cfg.find(' ', dev + 8);
  return "Gerät " + cfg.substr(dev + 8, dev_end - dev - 8) + " · Phase " + cfg.substr(phase + 7, 1);
}

inline std::string device_info(const Device &d) {
  if (d.config == nullptr)
    return "";
  return short_config(d.config->state) + " · " + num(d.wifi->state, "%.0f dBm") + " · v" + text_or(d.version->state);
}

// ---------------------------------------------------------------------------
// 24 h history
// ---------------------------------------------------------------------------
enum Channel { CH_CO2, CH_T, CH_RH, CH_LEVEL, CH_DEMAND, CH_EFF, CH_T_OUT, CH_T_IN, CH_T_SUP, CH_COUNT };
static const char *const CHANNEL_KEYS[CH_COUNT] = {"co2", "t", "rh", "level", "demand", "eff", "t_out", "t_in", "t_sup"};
static const int16_t NO_DATA = INT16_MIN;

enum class HistoryState { EMPTY, LOADING, OK, FAILED };

struct History {
  HistoryState state = HistoryState::EMPTY;
  int device = -1;
  uint32_t interval_s = 300;
  std::array<std::vector<int16_t>, CH_COUNT> series;

  size_t size() const { return this->series[CH_LEVEL].size(); }
  bool has(Channel c) const {
    for (int16_t v : this->series[c])
      if (v != NO_DATA)
        return true;
    return false;
  }
  // Newest non-empty value (stored fixed-point), NO_DATA if none
  int16_t latest(Channel c) const {
    for (auto it = this->series[c].rbegin(); it != this->series[c].rend(); ++it)
      if (*it != NO_DATA)
        return *it;
    return NO_DATA;
  }
};

inline History history;

inline std::string history_url() {
  const Device &d = current();
  if (d.ip == nullptr || missing(d.ip->state))
    return "";
  return "http://" + d.ip->state + "/history";
}

// Parses one '"key":[1,null,...]' array of the "series" object.
inline bool parse_series(const std::string &body, size_t from, const char *key, std::vector<int16_t> &out) {
  const std::string pattern = std::string("\"") + key + "\":[";
  size_t p = body.find(pattern, from);
  if (p == std::string::npos)
    return false;
  p += pattern.size();
  out.clear();
  const char *s = body.c_str();
  while (p < body.size() && s[p] != ']') {
    if (body.compare(p, 4, "null") == 0) {
      out.push_back(NO_DATA);
      p += 4;
    } else {
      char *end = nullptr;
      const long v = std::strtol(s + p, &end, 10);
      if (end == s + p)
        return false;
      out.push_back(static_cast<int16_t>(v));
      p = end - s;
    }
    if (p < body.size() && s[p] == ',')
      p++;
  }
  return p < body.size();
}

inline void history_parse(int device, int status, const std::string &body) {
  history.device = device;
  history.state = HistoryState::FAILED;
  if (status != 200)
    return;
  const size_t series = body.find("\"series\"");
  if (series == std::string::npos)
    return;
  const size_t iv = body.find("\"interval_s\":");
  if (iv != std::string::npos)
    history.interval_s = std::strtoul(body.c_str() + iv + 13, nullptr, 10);
  for (int c = 0; c < CH_COUNT; c++) {
    if (!parse_series(body, series, CHANNEL_KEYS[c], history.series[c]))
      return;
  }
  history.state = HistoryState::OK;
}

// ---------------------------------------------------------------------------
// Chart (LVGL lv_chart, enabled with -DLV_USE_CHART=1)
// ---------------------------------------------------------------------------
enum ChartTab { TAB_VENTILATION = 0, TAB_TEMPERATURE = 1 };
inline int chart_tab = TAB_VENTILATION;

static const uint32_t COL_GRID = 0x1A2028;  // the panel renders dark greys much brighter
static const uint32_t COL_DEMAND = 0xA78BFA;
static const uint32_t COL_CO2 = 0xF472B6;

struct ChartUi {
  lv_obj_t *box;        // container the chart is created in
  lv_obj_t *legend[4];  // legend labels
  lv_obj_t *y_max;
  lv_obj_t *y_min;
  lv_obj_t *note;       // time span / loading / error
  lv_obj_t *tab_vent;
  lv_obj_t *tab_temp;
};

struct ChartSeries {
  Channel channel;
  const char *name;
  const char *fmt;
  float scale;  // stored = value * scale
  uint32_t color;
  bool secondary;
};

inline void set_checked(lv_obj_t *o, bool on) {
  if (on)
    lv_obj_add_state(o, LV_STATE_CHECKED);
  else
    lv_obj_remove_state(o, LV_STATE_CHECKED);
}

inline void render_chart(const ChartUi &ui) {
  // Primary-axis series 0..2, secondary-axis series 3..4 (axis is fixed per lv_chart series)
  static lv_obj_t *chart = nullptr;
  static lv_chart_series_t *ser[5];
  if (chart == nullptr) {
    chart = lv_chart_create(ui.box);
    lv_obj_set_size(chart, LV_PCT(100), LV_PCT(100));
    lv_obj_center(chart);
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 5, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, lv_color_hex(COL_GRID), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 0, LV_PART_INDICATOR);
    for (int i = 0; i < 5; i++)
      ser[i] = lv_chart_add_series(chart, lv_color_hex(COL_TEXT),
                                   i < 3 ? LV_CHART_AXIS_PRIMARY_Y : LV_CHART_AXIS_SECONDARY_Y);
  }

  const bool loaded = history.state == HistoryState::OK && history.device == selected && history.size() >= 2;
  const bool has_temps = loaded && (history.has(CH_T_OUT) || history.has(CH_T_IN) || history.has(CH_T_SUP));
  if (!has_temps)
    chart_tab = TAB_VENTILATION;
  set_checked(ui.tab_vent, chart_tab == TAB_VENTILATION);
  set_checked(ui.tab_temp, chart_tab == TAB_TEMPERATURE);
  if (has_temps)
    lv_obj_remove_state(ui.tab_temp, LV_STATE_DISABLED);
  else
    lv_obj_add_state(ui.tab_temp, LV_STATE_DISABLED);

  // Series per tab: slots 0..2 primary axis, 3..4 secondary axis (nullptr = unused)
  static const ChartSeries VENT_LEVEL{CH_LEVEL, "Stufe", "%.0f", 10, COL_ACCENT, false};
  static const ChartSeries VENT_DEMAND{CH_DEMAND, "Bedarf", "%.0f %%", 10, COL_DEMAND, false};  // 0..1 -> 0..100 %
  static const ChartSeries VENT_CO2{CH_CO2, "CO2", "%.0f ppm", 1, COL_CO2, true};
  static const ChartSeries TEMP_OUT{CH_T_OUT, "Außen", "%.1f°", 10, COL_ZULUFT, false};
  static const ChartSeries TEMP_SUP{CH_T_SUP, "Zuluft", "%.1f°", 10, COL_ACCENT, false};
  static const ChartSeries TEMP_IN{CH_T_IN, "Abluft", "%.1f°", 10, COL_ABLUFT, false};
  static const ChartSeries TEMP_EFF{CH_EFF, "WRG", "%.0f %%", 10, COL_OK, true};
  const ChartSeries *slots[5];
  if (chart_tab == TAB_VENTILATION) {
    const ChartSeries *s[5] = {&VENT_LEVEL, &VENT_DEMAND, nullptr, &VENT_CO2, nullptr};
    std::copy(s, s + 5, slots);
  } else {
    const ChartSeries *s[5] = {&TEMP_OUT, &TEMP_SUP, &TEMP_IN, &TEMP_EFF, nullptr};
    std::copy(s, s + 5, slots);
  }

  const size_t n = loaded ? history.size() : 0;
  lv_chart_set_point_count(chart, n > 0 ? n : 1);

  // Primary axis: fixed 0..100 for level/demand, auto range for temperatures
  int32_t pmin = 0, pmax = 100;
  if (chart_tab == TAB_TEMPERATURE) {
    pmin = INT32_MAX;
    pmax = INT32_MIN;
    for (int i = 0; i < 3; i++) {
      if (slots[i] == nullptr || n == 0)
        continue;
      for (int16_t v : history.series[slots[i]->channel]) {
        if (v == NO_DATA)
          continue;
        pmin = std::min<int32_t>(pmin, v);
        pmax = std::max<int32_t>(pmax, v);
      }
    }
    if (pmin > pmax) {
      pmin = 0;
      pmax = 300;
    }
    pmin = (pmin / 10 - 1) * 10;  // whole degrees with 1 °C margin
    pmax = (pmax / 10 + 2) * 10;
  }
  lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, pmin, pmax);

  int32_t smin = 0, smax = 1000;  // efficiency 0..100 % (scale 10)
  if (chart_tab == TAB_VENTILATION) {
    smin = 400;
    smax = 2000;  // CO2 ppm
  }
  lv_chart_set_axis_range(chart, LV_CHART_AXIS_SECONDARY_Y, smin, smax);

  int legend = 0;
  for (int i = 0; i < 5; i++) {
    const ChartSeries *cs = slots[i];
    const bool visible = cs != nullptr && n > 0 && history.has(cs->channel);
    lv_chart_hide_series(chart, ser[i], !visible);
    if (!visible) {
      lv_chart_set_all_values(chart, ser[i], LV_CHART_POINT_NONE);
      continue;
    }
    lv_chart_set_series_color(chart, ser[i], lv_color_hex(cs->color));
    const auto &values = history.series[cs->channel];
    for (size_t p = 0; p < n; p++) {
      int32_t v = values[p];
      // Demand is stored ×1000 (0..1): show it as 0..100 like the level axis
      if (v != NO_DATA && cs->channel == CH_DEMAND)
        v /= 10;
      lv_chart_set_series_value_by_id(chart, ser[i], p, v == NO_DATA ? LV_CHART_POINT_NONE : v);
    }
    if (legend < 4) {
      const int16_t last = history.latest(cs->channel);
      float value = last / cs->scale;
      if (cs->channel == CH_DEMAND)
        value = last / 10.0f;
      // The newest bucket is a 5-min mean (e.g. 1.6 after 3 -> 1): show the level the unit runs now
      if (cs->channel == CH_LEVEL && current().level != nullptr && !std::isnan(current().level->state))
        value = current().level->state;
      set_text(ui.legend[legend], std::string(cs->name) + " " + num(value, cs->fmt));
      set_color(ui.legend[legend], cs->color);
      legend++;
    }
  }
  for (; legend < 4; legend++)
    set_text(ui.legend[legend], "");
  lv_chart_refresh(chart);

  if (chart_tab == TAB_VENTILATION) {
    set_text(ui.y_max, "10");
    set_text(ui.y_min, "0");
  } else {
    set_text(ui.y_max, num(pmax / 10.0f, "%.0f°"));
    set_text(ui.y_min, num(pmin / 10.0f, "%.0f°"));
  }

  switch (history.state) {
    case HistoryState::LOADING:
      set_text(ui.note, "Verlauf wird geladen …");
      break;
    case HistoryState::FAILED:
      set_text(ui.note, "Verlauf nicht erreichbar");
      break;
    case HistoryState::EMPTY:
      set_text(ui.note, "Kein Verlauf");
      break;
    case HistoryState::OK: {
      if (n < 2) {
        set_text(ui.note, "Noch keine Verlaufsdaten");
        break;
      }
      const uint32_t minutes = n * history.interval_s / 60;
      if (minutes >= 24 * 60) {
        set_text(ui.note, "letzte 24 h");
      } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "letzte %u h %02u min", unsigned(minutes / 60), unsigned(minutes % 60));
        set_text(ui.note, buf);
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Room controls (mode / level are room-wide; sent to the room master)
// ---------------------------------------------------------------------------
// Keep in sync with the send_mode script in dashboard_main.yaml
static const char *const MODES[5] = {"Smart-Automatik", "Wärmerückgewinnung", "Durchlüften", "Stoßlüftung", "Aus"};

inline int mode_index(const std::string &mode) {
  for (int i = 0; i < 5; i++)
    if (mode == MODES[i])
      return i;
  return -1;
}

}  // namespace dash

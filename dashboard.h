#pragma once

// Helpers for the LVGL dashboard: NaN-safe formatting, change-only label updates
// and the per-device card logic. Called from the YAML lambdas.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include "lvgl.h"

namespace dash {

// Colour palette (keep in sync with the style_definitions in dashboard_ui.yaml)
static const uint32_t COL_TEXT = 0xFFFFFF;
static const uint32_t COL_MUTED = 0xA9B4C0;
static const uint32_t COL_ACCENT = 0x2DD4BF;
static const uint32_t COL_ZULUFT = 0x38BDF8;
static const uint32_t COL_ABLUFT = 0xF59E0B;
static const uint32_t COL_OK = 0x22C55E;
static const uint32_t COL_ALARM = 0xEF4444;

// Material Design Icons code points (font_icon_* in dashboard_ui.yaml)
static const char *const ICON_FAN = "\U000F0210";
static const char *const ICON_FAN_OFF = "\U000F081D";
static const char *const ICON_OFFLINE = "\U000F0319";
static const char *const ICON_ZULUFT = "\U000F0F9C";  // home-import-outline
static const char *const ICON_ABLUFT = "\U000F0F9B";  // home-export-outline
static const char *const ICON_HA = "\U000F07D0";
static const char *const ICON_PRESENT = "\U000F0004";
static const char *const ICON_ABSENT = "\U000F0012";

// ESPHome hands out typed widget pointers (lv_label_t *, lv_obj_t *); all start with lv_obj_t.
template<typename T> inline lv_obj_t *obj(T *w) { return reinterpret_cast<lv_obj_t *>(w); }

// Only touch the label when the text really changes, so LVGL does not redraw needlessly.
template<typename T> inline void set_text(T *w, const std::string &text) {
  const char *cur = lv_label_get_text(obj(w));
  if (cur == nullptr || std::strcmp(cur, text.c_str()) != 0)
    lv_label_set_text(obj(w), text.c_str());
}

template<typename T> inline void set_color(T *w, uint32_t rgb) {
  lv_obj_set_style_text_color(obj(w), lv_color_hex(rgb), LV_PART_MAIN);
}

inline std::string num(float v, const char *fmt, const char *fallback = "--") {
  if (std::isnan(v))
    return fallback;
  char buf[32];
  std::snprintf(buf, sizeof(buf), fmt, v);
  return buf;
}

// HA text states that mean "no value"
inline bool missing(const std::string &s) { return s.empty() || s == "unavailable" || s == "unknown"; }

inline std::string text_or(const std::string &s, const char *fallback = "--") { return missing(s) ? fallback : s; }

// VentoSync status texts use "→", which Roboto does not contain.
inline std::string ascii_arrows(std::string s) {
  const std::string arrow = "→";
  for (size_t pos = s.find(arrow); pos != std::string::npos; pos = s.find(arrow, pos))
    s.replace(pos, arrow.size(), "»");
  return s;
}

struct CardLabels {
  lv_obj_t *status;     // fan / fan-off / offline icon
  lv_obj_t *name;
  lv_obj_t *level;      // big level number
  lv_obj_t *level_max;  // "/10"
  lv_obj_t *dir_icon;
  lv_obj_t *dir_text;
  lv_obj_t *rpm;
  lv_obj_t *filter;
};

// fan_state: state of the fan.* entity ("on" / "off" / "unavailable")
// direction: "Zuluft (Rein)" / "Abluft (Raus)"
inline void refresh_card(const CardLabels &l, const std::string &fan_state, float level, const std::string &direction,
                         float rpm, float filter_days, bool filter_alarm) {
  const bool offline = missing(fan_state);
  const bool off = fan_state == "off";

  set_text(l.status, offline ? ICON_OFFLINE : (off ? ICON_FAN_OFF : ICON_FAN));
  set_color(l.status, offline ? COL_ALARM : (off ? COL_MUTED : COL_OK));
  set_color(l.name, offline ? COL_MUTED : COL_TEXT);

  if (offline) {
    set_text(l.level, "--");
  } else if (off) {
    set_text(l.level, "Aus");
  } else {
    set_text(l.level, num(level, "%.0f"));
  }
  set_color(l.level, offline || off ? COL_MUTED : COL_TEXT);
  set_text(l.level_max, offline || off ? "" : "/10");

  const bool zuluft = direction.rfind("Zuluft", 0) == 0;
  const bool abluft = direction.rfind("Abluft", 0) == 0;
  if (offline || off || (!zuluft && !abluft)) {
    set_text(l.dir_icon, "");
    set_text(l.dir_text, offline ? "offline" : (off ? "Stillstand" : "--"));
    set_color(l.dir_text, offline ? COL_ALARM : COL_MUTED);
  } else {
    set_text(l.dir_icon, zuluft ? ICON_ZULUFT : ICON_ABLUFT);
    set_text(l.dir_text, zuluft ? "Zuluft" : "Abluft");
    set_color(l.dir_icon, zuluft ? COL_ZULUFT : COL_ABLUFT);
    set_color(l.dir_text, zuluft ? COL_ZULUFT : COL_ABLUFT);
  }

  set_text(l.rpm, std::isnan(rpm) || offline ? std::string("-- U/min") : num(std::fabs(rpm), "%.0f U/min"));
  set_text(l.filter, filter_alarm ? std::string("Filter wechseln!") : "Filter " + num(filter_days, "%.0f") + " d");
  set_color(l.filter, filter_alarm ? COL_ALARM : COL_MUTED);
}

}  // namespace dash

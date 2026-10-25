#pragma once

// Rooms and ventilation units shown on the display, and the room-wise overview
// paging (up to CARDS_PER_PAGE unit cards per overview page, more pages per room
// as needed). Filled at boot by packages/ventosync_room.yaml and
// packages/ventosync_device.yaml.

#include <array>
#include <string>
#include <vector>
#include "esphome.h"
#include "dashboard.h"

namespace dash {

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
// A VentoSync room (ESP-NOW group): mode, level and Smart-Automatik are room-wide
// and come from / are sent to the room master (device ID 1).
struct Room {
  const char *title = nullptr;  // nullptr = slot unused
  const char *master_prefix = "";
  esphome::text_sensor::TextSensor *mode = nullptr;
  esphome::text_sensor::TextSensor *auto_status = nullptr;
  esphome::sensor::Sensor *level = nullptr;
  esphome::sensor::Sensor *override_remaining = nullptr;
  esphome::sensor::Sensor *override_duration = nullptr;
};

struct Device {
  const char *title = nullptr;  // nullptr = slot unused
  int room = -1;
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

static const int MAX_ROOMS = 6;
static const int MAX_DEVICES = 12;
inline std::array<Room, MAX_ROOMS> rooms;
inline std::array<Device, MAX_DEVICES> devices;

inline int selected = 0;          // device shown on page_detail
inline bool detail_open = false;  // page_detail is on screen (history auto refresh)

inline Device &current() { return devices[selected]; }

inline const Room *room_of(const Device &d) {
  if (d.room < 0 || d.room >= MAX_ROOMS || rooms[d.room].title == nullptr)
    return nullptr;
  return &rooms[d.room];
}

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
  const Room *r = room_of(d);
  return std::string(r != nullptr ? std::string(r->title) + " · " : "") + short_config(d.config->state) + " · " +
         num(d.wifi->state, "%.0f dBm") + " · v" + text_or(d.version->state);
}

// ---------------------------------------------------------------------------
// Overview paging
// ---------------------------------------------------------------------------
static const int CARDS_PER_PAGE = 3;

struct OverviewPage {
  int room;
  int page_in_room;  // 0-based
  int pages_in_room;
  int count;  // cards on this page
  int device[CARDS_PER_PAGE];
};

inline int overview_page = 0;  // index into overview_pages()
inline int slot_device[CARDS_PER_PAGE] = {-1, -1, -1};

// Rooms in registry order, each room's units in registry order, CARDS_PER_PAGE per page.
inline std::vector<OverviewPage> overview_pages() {
  std::vector<OverviewPage> pages;
  for (int r = 0; r < MAX_ROOMS; r++) {
    if (rooms[r].title == nullptr)
      continue;
    std::vector<int> units;
    for (int d = 0; d < MAX_DEVICES; d++)
      if (devices[d].title != nullptr && devices[d].room == r)
        units.push_back(d);
    const int page_count = std::max<int>(1, (units.size() + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE);
    for (int p = 0; p < page_count; p++) {
      OverviewPage page{r, p, page_count, 0, {-1, -1, -1}};
      for (size_t i = p * CARDS_PER_PAGE; i < units.size() && page.count < CARDS_PER_PAGE; i++)
        page.device[page.count++] = units[i];
      pages.push_back(page);
    }
  }
  return pages;
}

// Advances to the next overview page; false after the last one (then the other pages follow).
inline bool overview_next() {
  if (overview_page + 1 < static_cast<int>(overview_pages().size())) {
    overview_page++;
    return true;
  }
  return false;
}

struct OverviewUi {
  lv_obj_t *card[CARDS_PER_PAGE];
  CardLabels labels[CARDS_PER_PAGE];
  lv_obj_t *room_title;
  lv_obj_t *room_page;
  lv_obj_t *mode;
  lv_obj_t *auto_status;
  lv_obj_t *counter[3];  // page counters of page_main, page_wrg, page_system
};

inline void refresh_overview(const OverviewUi &ui) {
  const auto pages = overview_pages();
  const int total = pages.size() + 2;  // + page_wrg, page_system
  if (overview_page >= static_cast<int>(pages.size()))
    overview_page = 0;
  char buf[16];
  for (int i = 0; i < 3; i++) {
    std::snprintf(buf, sizeof(buf), "%d/%d", i == 0 ? overview_page + 1 : int(pages.size()) + i, total);
    set_text(ui.counter[i], buf);
  }
  if (pages.empty()) {
    for (int s = 0; s < CARDS_PER_PAGE; s++) {
      lv_obj_add_flag(ui.card[s], LV_OBJ_FLAG_HIDDEN);
      slot_device[s] = -1;
    }
    set_text(ui.room_title, "Keine Räume");
    return;
  }

  const OverviewPage &page = pages[overview_page];
  const Room &room = rooms[page.room];
  set_text(ui.room_title, room.title);
  if (page.pages_in_room > 1) {
    std::snprintf(buf, sizeof(buf), "%d/%d", page.page_in_room + 1, page.pages_in_room);
    set_text(ui.room_page, buf);
  } else {
    set_text(ui.room_page, "");
  }
  set_text(ui.mode, room.mode != nullptr ? text_or(room.mode->state) : "--");
  set_text(ui.auto_status, room.auto_status != nullptr ? ascii_arrows(text_or(room.auto_status->state)) : "");

  // Card positions: the 3-card grid, fewer cards centred
  static const int CARD_W = 262, GAP = 9, SCREEN_W = 820;
  const int row_w = page.count * CARD_W + (page.count - 1) * GAP;
  const int x0 = (SCREEN_W - row_w) / 2;
  for (int s = 0; s < CARDS_PER_PAGE; s++) {
    slot_device[s] = s < page.count ? page.device[s] : -1;
    if (slot_device[s] < 0) {
      lv_obj_add_flag(ui.card[s], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(ui.card[s], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(ui.card[s], x0 + s * (CARD_W + GAP));
    const Device &d = devices[slot_device[s]];
    set_text(ui.labels[s].name, d.title);
    if (d.fan == nullptr)
      continue;
    refresh_card(ui.labels[s], d.fan->state, d.level->state, d.direction->state, d.rpm->state, d.filter_days->state,
                 d.filter_alarm->state);
  }
}

// ---------------------------------------------------------------------------
// System page: table of all units
// ---------------------------------------------------------------------------
inline void set_cell(lv_obj_t *table, uint32_t row, uint32_t col, const std::string &text) {
  const char *cur = lv_table_get_cell_value(table, row, col);
  if (cur == nullptr || std::strcmp(cur, text.c_str()) != 0)
    lv_table_set_cell_value(table, row, col, text.c_str());
}

// table: the lv_obj_t of the ESPHome table widget (id(...)->obj). Row 0 is the header row from YAML.
inline void refresh_system_table(lv_obj_t *table) {
  uint32_t row = 1;
  for (const Device &d : devices) {
    if (d.title == nullptr || d.fan == nullptr)
      continue;
    if (lv_table_get_row_count(table) <= row)
      lv_table_set_row_count(table, row + 1);
    const Room *r = room_of(d);
    set_cell(table, row, 0, d.title);
    set_cell(table, row, 1, r != nullptr ? r->title : "--");
    set_cell(table, row, 2, short_config(d.config->state));
    set_cell(table, row, 3, num(d.wifi->state, "%.0f dBm"));
    set_cell(table, row, 4, text_or(d.version->state));
    row++;
  }
}

}  // namespace dash

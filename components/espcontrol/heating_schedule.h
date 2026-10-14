#pragma once

// Pure parsing, lookup, and in-memory state for the on-device weekly heating
// schedule. Home Assistant configures it through one compact text entity per
// weekday ("HH:MM=temperature;HH:MM=temperature;..."); this header owns the
// parsed schedule and applies it independently of Home Assistant so it keeps
// running when the panel is offline. See devices/diyless-thermostat-3/device/
// heating_schedule.yaml for how it is wired to the boiler's climate entity.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace espcontrol::heating_schedule {

constexpr int MAX_PERIODS_PER_DAY = 4;
constexpr int DAYS_PER_WEEK = 7;
constexpr int MINUTES_PER_DAY = 24 * 60;
// Anticipation never starts a warm-up more than this far ahead of its period.
constexpr int PREHEAT_MAX_LEAD_MINUTES = 4 * 60;
constexpr float WARMUP_RATE_MIN_C_PER_H = 0.2f;
constexpr float WARMUP_RATE_MAX_C_PER_H = 5.0f;
// Share of each observed warm-up blended into the learned rate.
constexpr float WARMUP_RATE_LEARNING_WEIGHT = 0.3f;
// Warm-ups slow down as more heat leaks outside. The learned rate refers to
// WARMUP_REFERENCE_DIFF_C between indoors and outdoors and scales linearly
// to zero at WARMUP_FULL_POWER_LIFT_C, the difference full heating can just
// hold (both estimated from a winter of this house's on/off history).
constexpr float WARMUP_REFERENCE_DIFF_C = 12.0f;
constexpr float WARMUP_FULL_POWER_LIFT_C = 48.0f;

struct SchedulePeriod {
  int minute_of_day = -1;
  float target_c = NAN;
};

struct DaySchedule {
  SchedulePeriod periods[MAX_PERIODS_PER_DAY];
  int count = 0;
};

struct ActivePeriod {
  bool valid = false;
  float target_c = NAN;
  // Identifies which period is active (the weekday it started on and its
  // start time), so callers can tell a still-running period apart from a
  // newly started one without comparing floating-point temperatures.
  int weekday = -1;
  int minute_of_day = -1;
  // True when this period is applied ahead of its start time to warm up.
  bool early = false;
};

struct UpcomingPeriod {
  bool valid = false;
  float target_c = NAN;
  int weekday = -1;
  int minute_of_day = -1;
  int minutes_until = -1;
};

struct HeatingScheduleState {
  DaySchedule days[DAYS_PER_WEEK];
  int last_applied_weekday = -1;
  int last_applied_minute = -1;
};

// Header-owned singleton so the parsed schedule survives independently of
// any single ESPHome component, mirroring climate_info_providers() in
// button_grid_climate.h.
inline HeatingScheduleState &heating_schedule_state() {
  static HeatingScheduleState state;
  return state;
}

// Converts ESPHome's ESPTime::day_of_week (1=Sunday..7=Saturday) to a
// Monday-first index (0=Monday..6=Sunday), matching how the schedule text
// entities are named (Monday..Sunday).
constexpr int monday_first_weekday(int esphome_day_of_week) {
  return (esphome_day_of_week + 5) % 7;
}

inline bool parse_two_digit(const std::string &text, size_t pos, int &value) {
  char tens = text[pos];
  char ones = text[pos + 1];
  if (tens < '0' || tens > '9' || ones < '0' || ones > '9') return false;
  value = (tens - '0') * 10 + (ones - '0');
  return true;
}

// Parses one "HH:MM=<float>" period, e.g. "06:30=20.5".
inline bool parse_period_token(const std::string &token, SchedulePeriod &out) {
  if (token.size() < 7 || token[2] != ':' || token[5] != '=') return false;
  int hour = 0, minute = 0;
  if (!parse_two_digit(token, 0, hour) || !parse_two_digit(token, 3, minute)) return false;
  if (hour > 23 || minute > 59) return false;

  const std::string temperature_text = token.substr(6);
  char *end = nullptr;
  float target_c = strtof(temperature_text.c_str(), &end);
  if (end == temperature_text.c_str() || !std::isfinite(target_c)) return false;

  out.minute_of_day = hour * 60 + minute;
  out.target_c = target_c;
  return true;
}

// Inserts or replaces the period at the same minute-of-day. Extra distinct
// periods beyond MAX_PERIODS_PER_DAY are dropped once the day is full.
inline void upsert_period(DaySchedule &schedule, const SchedulePeriod &period) {
  for (int i = 0; i < schedule.count; i++) {
    if (schedule.periods[i].minute_of_day == period.minute_of_day) {
      schedule.periods[i] = period;
      return;
    }
  }
  if (schedule.count < MAX_PERIODS_PER_DAY) schedule.periods[schedule.count++] = period;
}

// Parses a full day's compact schedule string ("HH:MM=temp;HH:MM=temp;...").
// Malformed periods are skipped rather than rejecting the whole string, so a
// single typo does not lose the rest of the day's schedule.
inline DaySchedule parse_day_schedule(const std::string &text) {
  DaySchedule schedule;
  size_t start = 0;
  while (start <= text.size()) {
    size_t sep = text.find(';', start);
    size_t token_len = sep == std::string::npos ? std::string::npos : sep - start;
    std::string token = text.substr(start, token_len);
    SchedulePeriod period;
    if (!token.empty() && parse_period_token(token, period)) upsert_period(schedule, period);
    if (sep == std::string::npos) break;
    start = sep + 1;
  }
  std::sort(schedule.periods, schedule.periods + schedule.count,
            [](const SchedulePeriod &a, const SchedulePeriod &b) {
              return a.minute_of_day < b.minute_of_day;
            });
  return schedule;
}

inline void set_day_schedule(int weekday, const std::string &text) {
  if (weekday < 0 || weekday >= DAYS_PER_WEEK) return;
  heating_schedule_state().days[weekday] = parse_day_schedule(text);
}

// Finds the temperature that should currently be active: the most recent
// period at or before `minute_of_day` today, falling back to the last period
// of the closest earlier day (looking back up to and including a full week,
// so a day that is the only one configured still carries over from its own
// last occurrence) so a day with no periods of its own carries over the last
// setting, and a reboot mid-period re-derives the correct target instead of
// losing it.
inline ActivePeriod find_active_period(const DaySchedule (&days)[DAYS_PER_WEEK],
                                       int weekday, int minute_of_day) {
  for (int back = 0; back <= DAYS_PER_WEEK; back++) {
    int day = ((weekday - back) % DAYS_PER_WEEK + DAYS_PER_WEEK) % DAYS_PER_WEEK;
    const DaySchedule &schedule = days[day];
    for (int i = schedule.count - 1; i >= 0; i--) {
      if (back == 0 && schedule.periods[i].minute_of_day > minute_of_day) continue;
      return ActivePeriod{true, schedule.periods[i].target_c, day, schedule.periods[i].minute_of_day};
    }
  }
  return ActivePeriod{};
}

// Finds the first period that starts strictly after `minute_of_day` on
// `weekday`, looking ahead up to a full week.
inline UpcomingPeriod find_next_period(const DaySchedule (&days)[DAYS_PER_WEEK],
                                       int weekday, int minute_of_day) {
  for (int ahead = 0; ahead <= DAYS_PER_WEEK; ahead++) {
    int day = (weekday + ahead) % DAYS_PER_WEEK;
    const DaySchedule &schedule = days[day];
    for (int i = 0; i < schedule.count; i++) {
      int minutes_until = ahead * MINUTES_PER_DAY + schedule.periods[i].minute_of_day - minute_of_day;
      if (minutes_until <= 0) continue;
      return UpcomingPeriod{true, schedule.periods[i].target_c, day,
                            schedule.periods[i].minute_of_day, minutes_until};
    }
  }
  return UpcomingPeriod{};
}

// Minutes needed to warm the room from `current_c` to `target_c` at
// `rate_c_per_h`, capped at PREHEAT_MAX_LEAD_MINUTES; 0 when no warm-up is
// needed or the inputs are unknown.
inline int preheat_lead_minutes(float current_c, float target_c, float rate_c_per_h) {
  if (!std::isfinite(current_c) || !std::isfinite(target_c) || !(rate_c_per_h > 0)) return 0;
  float rise_c = target_c - current_c;
  if (rise_c <= 0) return 0;
  int lead = static_cast<int>(std::ceil(rise_c / rate_c_per_h * 60.0f));
  return std::min(lead, PREHEAT_MAX_LEAD_MINUTES);
}

// How much faster (>1) or slower (<1) a warm-up runs at the given indoor and
// outdoor temperatures than at the reference difference; 1 when either is
// unknown.
inline float warmup_outdoor_factor(float indoor_c, float outdoor_c) {
  if (!std::isfinite(indoor_c) || !std::isfinite(outdoor_c)) return 1.0f;
  float diff_c = std::clamp(indoor_c - outdoor_c, 0.0f, WARMUP_FULL_POWER_LIFT_C - 8.0f);
  return (WARMUP_FULL_POWER_LIFT_C - diff_c) / (WARMUP_FULL_POWER_LIFT_C - WARMUP_REFERENCE_DIFF_C);
}

// Picks the period to apply now. Schedule times mean "warm by": once the
// next, warmer period is within its warm-up lead time it is applied early;
// otherwise the currently active period applies. Cooler periods are never
// anticipated.
inline ActivePeriod select_scheduled_period(const DaySchedule (&days)[DAYS_PER_WEEK],
                                            int weekday, int minute_of_day,
                                            float current_c, float outdoor_c,
                                            float reference_rate_c_per_h) {
  const float rate_c_per_h = reference_rate_c_per_h * warmup_outdoor_factor(current_c, outdoor_c);
  ActivePeriod active = find_active_period(days, weekday, minute_of_day);
  UpcomingPeriod next = find_next_period(days, weekday, minute_of_day);
  if (!next.valid || (active.valid && next.target_c <= active.target_c)) return active;
  if (next.minutes_until > preheat_lead_minutes(current_c, next.target_c, rate_c_per_h)) return active;
  return ActivePeriod{true, next.target_c, next.weekday, next.minute_of_day, true};
}

// Blends one observed warm-up (`rise_c` degrees in `elapsed_minutes`, run
// at `outdoor_factor` from warmup_outdoor_factor) into the learned reference
// rate. Warm-ups too small or short to measure reliably leave the rate
// unchanged.
inline float learned_warmup_rate(float current_rate, float rise_c, float elapsed_minutes,
                                 float outdoor_factor) {
  if (rise_c < 0.5f || elapsed_minutes < 15.0f || !(outdoor_factor > 0)) return current_rate;
  float observed = std::clamp(rise_c / (elapsed_minutes / 60.0f) / outdoor_factor,
                              WARMUP_RATE_MIN_C_PER_H, WARMUP_RATE_MAX_C_PER_H);
  return current_rate + WARMUP_RATE_LEARNING_WEIGHT * (observed - current_rate);
}

// True when `active` names a different period than the one last applied, so
// callers only issue a climate.control call at genuine period boundaries (or
// when the caller has just reset last_applied, e.g. after an edit).
constexpr bool schedule_period_changed(int last_weekday, int last_minute,
                                       int weekday, int minute_of_day) {
  return last_weekday != weekday || last_minute != minute_of_day;
}

}  // namespace espcontrol::heating_schedule

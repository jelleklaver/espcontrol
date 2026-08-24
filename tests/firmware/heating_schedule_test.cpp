#include <cassert>

#include "heating_schedule.h"

using espcontrol::heating_schedule::ActivePeriod;
using espcontrol::heating_schedule::DAYS_PER_WEEK;
using espcontrol::heating_schedule::DaySchedule;
using espcontrol::heating_schedule::MAX_PERIODS_PER_DAY;
using espcontrol::heating_schedule::SchedulePeriod;

namespace {

const int MONDAY = 0;
const int SUNDAY = 6;

}  // namespace

int main() {
  using espcontrol::heating_schedule::find_active_period;
  using espcontrol::heating_schedule::monday_first_weekday;
  using espcontrol::heating_schedule::parse_day_schedule;
  using espcontrol::heating_schedule::parse_period_token;
  using espcontrol::heating_schedule::schedule_period_changed;
  using espcontrol::heating_schedule::upsert_period;

  // ESPHome's ESPTime::day_of_week is 1=Sunday..7=Saturday; the schedule
  // uses a Monday-first index matching the Monday..Sunday entity names.
  assert(monday_first_weekday(1) == SUNDAY);
  assert(monday_first_weekday(2) == MONDAY);
  assert(monday_first_weekday(3) == 1);
  assert(monday_first_weekday(4) == 2);
  assert(monday_first_weekday(5) == 3);
  assert(monday_first_weekday(6) == 4);
  assert(monday_first_weekday(7) == 5);

  // Single valid period.
  {
    SchedulePeriod period;
    assert(parse_period_token("06:30=20.5", period));
    assert(period.minute_of_day == 6 * 60 + 30);
    assert(period.target_c == 20.5f);
  }

  // Negative and whole-number temperatures parse too.
  {
    SchedulePeriod period;
    assert(parse_period_token("23:00=-5", period));
    assert(period.minute_of_day == 23 * 60);
    assert(period.target_c == -5.0f);
  }

  // Malformed tokens are rejected outright rather than partially accepted.
  {
    SchedulePeriod period;
    assert(!parse_period_token("6:30=20.5", period));   // hour not zero-padded
    assert(!parse_period_token("24:00=20", period));    // hour out of range
    assert(!parse_period_token("12:60=20", period));    // minute out of range
    assert(!parse_period_token("12:00-20", period));    // missing '='
    assert(!parse_period_token("12:00=", period));       // no temperature
    assert(!parse_period_token("12:00=abc", period));    // non-numeric temperature
  }

  // A full day string, unsorted, with one malformed entry that must be
  // skipped without discarding the rest of the day.
  {
    DaySchedule schedule = parse_day_schedule("17:00=21;06:30=20.5;bad;22:00=16");
    assert(schedule.count == 3);
    assert(schedule.periods[0].minute_of_day == 6 * 60 + 30);
    assert(schedule.periods[1].minute_of_day == 17 * 60);
    assert(schedule.periods[2].minute_of_day == 22 * 60);
  }

  // Repeating the same minute-of-day overwrites rather than appending.
  {
    DaySchedule schedule = parse_day_schedule("06:30=20.5;06:30=19");
    assert(schedule.count == 1);
    assert(schedule.periods[0].target_c == 19.0f);
  }

  // Entries beyond MAX_PERIODS_PER_DAY are dropped, not overflowed.
  {
    DaySchedule schedule = parse_day_schedule("00:00=1;01:00=2;02:00=3;03:00=4;04:00=5");
    assert(schedule.count == MAX_PERIODS_PER_DAY);
    assert(schedule.periods[MAX_PERIODS_PER_DAY - 1].minute_of_day == 3 * 60);
  }

  // Empty string yields an empty (disabled) day.
  {
    DaySchedule schedule = parse_day_schedule("");
    assert(schedule.count == 0);
  }

  // Lookup: exact match at a period's start time.
  {
    DaySchedule days[DAYS_PER_WEEK] = {};
    days[MONDAY] = parse_day_schedule("06:30=20.5;17:00=21;22:00=16");
    ActivePeriod active = find_active_period(days, MONDAY, 17 * 60);
    assert(active.valid);
    assert(active.target_c == 21.0f);
    assert(active.weekday == MONDAY);
    assert(active.minute_of_day == 17 * 60);
  }

  // Lookup: between two periods returns the earlier one.
  {
    DaySchedule days[DAYS_PER_WEEK] = {};
    days[MONDAY] = parse_day_schedule("06:30=20.5;17:00=21;22:00=16");
    ActivePeriod active = find_active_period(days, MONDAY, 12 * 60);
    assert(active.valid);
    assert(active.target_c == 20.5f);
  }

  // Lookup: before today's first period carries over from the most recent
  // earlier day with any periods at all.
  {
    DaySchedule days[DAYS_PER_WEEK] = {};
    days[SUNDAY] = parse_day_schedule("22:00=16");
    ActivePeriod active = find_active_period(days, MONDAY, 5 * 60);
    assert(active.valid);
    assert(active.target_c == 16.0f);
    assert(active.weekday == SUNDAY);
  }

  // Lookup: a sparsely configured week (only Monday has periods) carries
  // that day's last period all the way around to the following Monday.
  {
    DaySchedule days[DAYS_PER_WEEK] = {};
    days[MONDAY] = parse_day_schedule("06:30=20.5;22:00=16");
    for (int weekday = 0; weekday < DAYS_PER_WEEK; weekday++) {
      ActivePeriod active = find_active_period(days, weekday, 3 * 60);
      assert(active.valid);
      assert(active.target_c == 16.0f);
    }
  }

  // Lookup: no periods anywhere in the week is explicitly invalid.
  {
    DaySchedule days[DAYS_PER_WEEK] = {};
    ActivePeriod active = find_active_period(days, MONDAY, 12 * 60);
    assert(!active.valid);
  }

  // Change detection identifies a period by (weekday, minute_of_day), not
  // by the temperature it carries.
  assert(schedule_period_changed(-1, -1, MONDAY, 6 * 60 + 30));
  assert(!schedule_period_changed(MONDAY, 6 * 60 + 30, MONDAY, 6 * 60 + 30));
  assert(schedule_period_changed(MONDAY, 6 * 60 + 30, MONDAY, 17 * 60));
  assert(schedule_period_changed(SUNDAY, 22 * 60, MONDAY, 22 * 60));

  return 0;
}

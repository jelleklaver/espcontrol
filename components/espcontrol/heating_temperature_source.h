#pragma once

// Follows a Home Assistant temperature entity chosen at runtime (a sensor's
// state or a weather entity's temperature attribute) for device heating logic.

// Internal implementation detail for button_grid.h. Include button_grid.h from device YAML.

// Live reading of one configured Home Assistant temperature entity. Home
// Assistant only pushes changes, so `updated_ms` is the time of the last
// change, not of the last report: a stable temperature can stay quiet for a
// long time. `value` is NAN while the entity is unavailable or unknown.
struct HaTemperatureSource {
  std::string subscribed_entity;
  uint32_t subscribed_generation = 0;
  float value = NAN;
  uint32_t updated_ms = 0;
};

// True when `source` has a reading no older than `max_age_ms`.
inline bool ha_temperature_fresh(const HaTemperatureSource &source, uint32_t max_age_ms) {
  return !std::isnan(source.value) && source.updated_ms != 0 &&
         esphome::millis() - source.updated_ms < max_age_ms;
}

// (Re)subscribes `source` to `configured` when it, the Home Assistant
// connection or the subscription generation changed. Entities outside the
// sensor and weather domains are ignored. A replaced entity's last reading
// is forgotten so it can never be mistaken for the new one. A non-numeric
// state (unavailable, unknown) clears the reading. `on_update` runs after
// every change of reading.
inline void refresh_ha_temperature_source(HaTemperatureSource &source,
                                          const std::string &configured,
                                          std::function<void()> on_update) {
  const bool supported_domain =
      configured.rfind("sensor.", 0) == 0 || configured.rfind("weather.", 0) == 0;
  const std::string entity =
      ha_api_state_connected() && supported_domain ? configured : std::string();
  const uint32_t generation = entity.empty() ? 0 : ha_subscription_generation();
  if (entity == source.subscribed_entity && generation == source.subscribed_generation) return;
  ha_release_callbacks_for_owner(&source);
  if (entity != source.subscribed_entity) {
    source.value = NAN;
    source.updated_ms = 0;
  }
  source.subscribed_entity = entity;
  source.subscribed_generation = generation;
  if (entity.empty()) return;
  {
    HaCallbackOwnerScope scope(&source);
    const bool subscribed = ha_subscribe_attribute(
        entity, ha_temperature_attribute(entity),
        std::function<void(esphome::StringRef)>(
            [&source, entity, generation, on_update](esphome::StringRef state) {
              if (generation != ha_subscription_generation() ||
                  entity != source.subscribed_entity) return;
              float reading = NAN;
              if (!parse_float_ref(state, reading)) reading = NAN;
              source.value = reading;
              source.updated_ms = std::isnan(reading) ? 0 : esphome::millis();
              if (on_update) on_update();
            }));
    if (!subscribed) {
      source.subscribed_entity.clear();
      source.subscribed_generation = 0;
      return;
    }
  }
  ha_reannounce_state_subscriptions();
}

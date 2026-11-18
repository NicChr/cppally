#ifndef CPPALLY_ZONE_H
#define CPPALLY_ZONE_H

#include <cppally/r_setup.h>
#include <cppally/scalar/r_str.h>
#include <cppally/time/tz_fwd.h>
#include <R_ext/Visibility.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Time-zone utilities for r_psxct.h
// Handle loading tzdb and getting the session time-zone.
// On first time-zone usage, lazy-parse the IANA data embedded in tzdata.h.
// For each first-usage of a distinct time-zone, lazily build its offset table.
// Important: for multi-threading to work safely, session_name must be cached (on package load).
// This header is depended on by r_vector.h, so we have to use R C API for vector operations.
// Only the hot path lives here: everything needing tz.h is compiled once per package, in the translation unit
// that defines CPPALLY_COMPILE_ONCE (the generated cppally.cpp), see zone_impl.h

namespace cppally {

namespace internal {

struct no_zone {};

// A zone's UTC offset history from tzdb.
// Period i covers POSIX seconds [starts[i], starts[i + 1]) with offsets[i] in force
struct attribute_hidden offset_table {

  // Tabulated up to the first transition after this year, later instants fall back to tzdb
  static constexpr int horizon_year = 2100;
  // 2^20 second (~12 day) buckets, rarely holding more than one transition
  static constexpr int bucket_shift = 20;

  std::vector<int64_t> starts; // One per period plus the end of the last, starts[0] is the minimum
  std::vector<int32_t> offsets;
  std::vector<uint16_t> abbrev_ids; // Index into abbrevs
  std::vector<std::string> abbrevs;
  std::vector<uint16_t> buckets; // Period in force at starts[1] + (k << bucket_shift)

  offset_table() = default;

  // Not thread-safe, zone_registry serialises calls
  explicit offset_table(const tz::time_zone& zone);

  // Period associated with POSIX second, -1 if it's beyond the table
  int period(int64_t s) const noexcept {
    const size_t last = offsets.size() - 1;
    if (s >= starts[last]) {
      return s < starts[last + 1] ? static_cast<int>(last) : -1;
    }
    if (s < starts[1]) {
      return 0;
    }
    size_t i = buckets[static_cast<size_t>((s - starts[1]) >> bucket_shift)];
    while (s >= starts[i + 1]) {
      ++i;
    }
    return static_cast<int>(i);
  }

  private:

  uint16_t abbrev_id(const std::string& abbrev);
};

// One for each unique time-zone name. `tzdb_zone` is nullptr for an unknown name (or a missing database)
// date-times for an unknown zone are NA
struct zone_record {
  std::string name;
  const tz::time_zone* tzdb_zone = nullptr; // tzdb's definition of the zone
  offset_table table; // Built from `tzdb_zone` when it's resolved
  zone_record* next = nullptr; // Registry list

  bool known() const noexcept {
    return tzdb_zone != nullptr;
  }
};

// Thread-safe time-zone registry.
// The only part that is not thread-safe is the session zone, which is cached on package load
// A linked list is used to support r_psxct construction by name in parallel. Other threads need to be able to read the list without a lock while another thread is adding to it.
struct attribute_hidden zone_registry {

  // One registry per dll
  static zone_registry& get() {
    static zone_registry& r = *new zone_registry;
    return r;
  }

  // Returns nullptr for UTC, "" is the session zone
  const zone_record* find_or_add(std::string_view name) {
    if (name == "UTC") {
      return nullptr;
    }
    if (const zone_record* zone = find(name)) {
      return zone;
    }
    return add(name);
  }

  // NA is the session zone, like ""
  const zone_record* find_or_add(r_str_view name) {
    return find_or_add(name.is_na() ? std::string_view() : name.cpp_str());
  }

  // Main thread: cache the session zone
  void cache_r_components();

  // Main thread: Update a stale session zone
  void update_session();

  private:

  std::atomic<zone_record*> head{nullptr};  // Linked list
  std::string session_name;                 // Cached session name
  bool r_components_done = false;           // Whether the session zone has been cached
  bool db_failed = false;                   // Whether the embedded db failed to parse

  const zone_record* find(std::string_view name) const noexcept {
    // Start at newest record, traversing older records until zone name is found or end of list
    for (const zone_record* zone = head.load(std::memory_order_acquire); zone; zone = zone->next) {
      if (zone->name == name) {
        return zone;
      }
    }
    return nullptr;
  }

  // Defined in zone_impl.h, along with the mutex that serialises them (kept there so <mutex> stays out of this header)
  // Creates and publishes the record for `name`, unless another thread got there first
  const zone_record* add(std::string_view name);
  bool needs_r_components();
  void resolve(zone_record& zone) noexcept;
  bool ensure_db();
  static std::string find_session_zone_name();
};

// tzdb lookups for instants beyond a zone's offset_table, kept out of line off the hot path
attribute_hidden bool tzdb_utc_offset(const zone_record* zone, std::chrono::sys_seconds tp, std::chrono::seconds& out, std::string* abbrev) noexcept;
attribute_hidden bool tzdb_local_to_sys(const zone_record* zone, tz::local_seconds tp, std::chrono::sys_seconds& out) noexcept;

// UTC offset in force at `tp` (0 for UTC), plus its abbreviation if `abbrev` is given. False for an unknown zone or if the lookup fails
inline bool utc_offset(const zone_record* zone, std::chrono::sys_seconds tp, std::chrono::seconds& out, std::string* abbrev = nullptr) noexcept {
  try {
    if (!zone) {
      out = std::chrono::seconds{0};
      if (abbrev) {
        *abbrev = "UTC";
      }
      return true;
    }
    if (!zone->known()) {
      return false;
    }
    const int i = zone->table.period(tp.time_since_epoch().count());
    if (i >= 0) [[likely]] {
      out = std::chrono::seconds{zone->table.offsets[i]};
      if (abbrev) {
        *abbrev = zone->table.abbrevs[zone->table.abbrev_ids[i]];
      }
      return true;
    }
  } catch (...) {
    return false;
  }
  return tzdb_utc_offset(zone, tp, out, abbrev);
}

// Local wall-clock instant. Returns false if non-existant or ambiguous, or if zone is unknown or lookup fails.
inline bool local_to_sys(const zone_record* zone, tz::local_seconds tp, std::chrono::sys_seconds& out) noexcept {
  if (!zone) {
    out = std::chrono::sys_seconds{tp.time_since_epoch()};
    return true;
  }
  if (!zone->known()) {
    return false;
  }
  const offset_table& table = zone->table;
  const int64_t local = tp.time_since_epoch().count();

  const int lo = table.period(local - 86400);
  const int hi = table.period(local + 86400);
  if (lo >= 0 && hi >= 0) [[likely]] {
    int matches = 0;
    int64_t sys = 0;
    for (int i = lo; i <= hi; ++i) {
      const int64_t s = local - table.offsets[i];
      if (s >= table.starts[i] && s < table.starts[i + 1]) {
        sys = s;
        ++matches;
      }
    }
    // 0 matches is a gap (nonexistent), 2 an overlap (ambiguous)
    if (matches != 1) {
      return false;
    }
    out = std::chrono::sys_seconds{std::chrono::seconds{sys}};
    return true;
  }
  return tzdb_local_to_sys(zone, tp, out);
}

}

// Resync cppally's session time-zone to the current system time-zone
inline void resync_session_tz() {
  internal::zone_registry::get().update_session();
}

}

#ifdef CPPALLY_COMPILE_ONCE
#include <cppally/time/zone_impl.h>
#endif

#endif

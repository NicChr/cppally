#ifndef CPPALLY_ZONE_H
#define CPPALLY_ZONE_H

#include <cppally/r_setup.h>
#include <cppally/time/tz_fwd.h>
#include <cppally/time/roll.h>
#include <R_ext/Visibility.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

// Time-zone utilities for r_psxct.h
// Handle loading tzdb and getting the session time-zone.
// On first time-zone usage, lazy-parse the IANA data embedded in tzdata.h.
// For each first-usage of a distinct time-zone, lazily build its offset table.
// Important: for the session zone to work in parallel, Sys.timezone() must be cached (on package load).
// This header is depended on by r_vector.h, so we have to use R C API for vector operations.
// Only the hot path lives here: everything needing tz.h is compiled once per package, in the translation unit
// that defines CPPALLY_COMPILE_ONCE (the generated cppally.cpp), see zone_impl.h

namespace cppally {

namespace internal {

struct no_zone {};

// A zone's UTC offset history from tzdb.
// Period i covers POSIX seconds [starts[i], starts[i + 1]) with offsets[i] in force
struct attribute_hidden offset_table {

  // Minimum extent of the table, later instants fall back to tzdb unless the zone's offsets cycle
  static constexpr int horizon_year = 2100;
  // 2^20 second (~12 day) buckets, rarely holding more than one transition
  static constexpr int bucket_shift = 20;
  // The Gregorian calendar repeats every 400 years (146097 days), so do offsets once a zone's rules stop changing
  static constexpr int64_t cycle_seconds = int64_t{146097} * 86400;

  std::vector<int64_t> starts; // One per period plus the end of the last, starts[0] is the minimum
  std::vector<int32_t> offsets;
  std::vector<uint16_t> abbrev_ids; // Index into abbrevs
  std::vector<std::string> abbrevs;
  std::vector<uint16_t> buckets; // Period in force at starts[1] + (k << bucket_shift)
  int64_t cycle_end = std::numeric_limits<int64_t>::max(); // The table's final 400 years before this repeat forever

  offset_table() = default;

  // Not thread-safe, zone_registry serialises calls
  explicit offset_table(const tz::time_zone& zone);

  // Whole cycles to subtract from an instant past cycle_end to land it in the table's final 400 years
  int64_t cycle_shift(int64_t s) const noexcept {
    if (s < cycle_end) [[likely]] {
      return 0;
    }
    return ((s - cycle_end) / cycle_seconds + 1) * cycle_seconds;
  }

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

// One for each unique time-zone name except UTC, which is nullptr.
// `tzdb_zone` is nullptr if unresolved.
// An unresolved `tzdb_zone` can occur if the db rejects the timezone, or there is no database.
struct zone_record {
  std::string name;
  std::string session_name; // Zone a session record ("") resolved to
  const tz::time_zone* tzdb_zone = nullptr; // tzdb's definition of the zone
  offset_table table; // Built from `tzdb_zone` when it's resolved
  zone_record* next = nullptr; // Registry list
  bool invalid = false; // Rejected by the database

  bool known() const noexcept {
    return tzdb_zone != nullptr;
  }
};

// Thread-safe time-zone registry.
// The session zone is TZ if set, else Sys.timezone(), which needs R and so is cached on package load.
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
    if (name.empty()) {
      return session_zone();
    }
    if (const zone_record* zone = find(name)) {
      return zone;
    }
    return add(name, std::string_view());
  }

  // Main thread: cache Sys.timezone(), the session zone when TZ is unset
  void cache_session_zone();

  private:

  std::atomic<zone_record*> head{nullptr};  // Linked list
  std::string sys_timezone;                 // Cached Sys.timezone(), never changed once cached
  std::atomic<bool> session_cached{false};  // Whether sys_timezone has been cached
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

  const zone_record* find_session(std::string_view session_name) const noexcept {
    for (const zone_record* zone = head.load(std::memory_order_acquire); zone; zone = zone->next) {
      if (zone->name.empty() && zone->session_name == session_name) {
        return zone;
      }
    }
    return nullptr;
  }

  // Defined in zone_impl.h, along with the mutex that serialises them (kept there so <mutex> stays out of this header)
  // Creates and publishes the record for `name` (or the session record for `session_name`), unless another thread got there first
  const zone_record* add(std::string_view name, std::string_view session_name);
  const zone_record* session_zone();
  void resolve(zone_record& zone) noexcept;
  bool ensure_db();
  static std::string find_sys_timezone();
};

// tzdb lookups for instants beyond a zone's offset_table, kept out of line off the hot path
attribute_hidden bool tzdb_utc_offset(const zone_record* zone, std::chrono::sys_seconds tp, std::chrono::seconds& out, std::string* abbrev) noexcept;
attribute_hidden bool tzdb_local_to_sys(const zone_record* zone, tz::local_seconds tp, std::chrono::sys_seconds& out, roll on_impossible, roll on_ambiguous) noexcept;

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
    int64_t s = tp.time_since_epoch().count();
    s -= zone->table.cycle_shift(s);
    const int i = zone->table.period(s);
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

// Local wall-clock instant. A nonexistent reading rolls to the DST transition and an ambiguous one to the earlier/later instant,
// unless its roll is none. Returns false for an unresolved reading, an unknown zone or a failed lookup.
// away/nearest act as if adding, callers with a direction resolve them first
inline bool local_to_sys(const zone_record* zone, tz::local_seconds tp, std::chrono::sys_seconds& out,
                         roll on_impossible = roll::none, roll on_ambiguous = roll::none) noexcept {
  if (!zone) {
    out = std::chrono::sys_seconds{tp.time_since_epoch()};
    return true;
  }
  if (!zone->known()) {
    return false;
  }
  const offset_table& table = zone->table;
  int64_t local = tp.time_since_epoch().count();
  // Solved within the table's final 400 years, then shifted back
  const int64_t shift = table.cycle_shift(local);
  local -= shift;

  int lo = table.period(local - 86400);
  int hi = table.period(local + 86400);
  if (lo >= 0 && hi >= 0) [[likely]] {
    int matches = 0; // 1 = unique, 0 = gap (non-existent), 2 = overlap (ambiguous)
    int64_t earliest = 0;
    int64_t latest = 0;
    for (int i = lo; i <= hi; ++i) {
      int64_t s = local - table.offsets[i];
      if (s >= table.starts[i] && s < table.starts[i + 1]) {
        if (matches == 0) {
          earliest = s;
        }
        latest = s;
        ++matches;
      }
    }
    if (matches == 1) [[likely]] {
      out = std::chrono::sys_seconds{std::chrono::seconds{earliest + shift}};
      return true;
    }
    // Overlap (ambiguous)
    if (matches > 1) {
      on_ambiguous = resolve_roll(on_ambiguous, /*forward=*/ true);
      if (on_ambiguous == roll::none) {
        return false;
      }
      out = std::chrono::sys_seconds{std::chrono::seconds{(on_ambiguous == roll::forward ? latest : earliest) + shift}};
      return true;
    }
    // Gap (nonexistent): the reading is past the end of period i - 1 but before the start of period i
    if (on_impossible == roll::none) {
      return false;
    }
    for (int i = lo + 1; i <= hi; ++i) {
      if (local - table.offsets[i - 1] >= table.starts[i] && local - table.offsets[i] < table.starts[i]) {
        out = std::chrono::sys_seconds{std::chrono::seconds{table.starts[i] + shift}};
        return true;
      }
    }
    return false;
  }
  return tzdb_local_to_sys(zone, tp, out, on_impossible, on_ambiguous);
}

}

}

#ifdef CPPALLY_COMPILE_ONCE
#include <cppally/time/zone_impl.h>
#endif

#endif

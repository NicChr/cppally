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

  // Minimum extent of the table, beyond this, a DST zone's offsets repeat and a fixed-offset zone's last period is extended indefinitely
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

  [[gnu::cold]] explicit offset_table(const tz::time_zone& zone);

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

// One for each unique time-zone name except UTC, which is nullptr, plus one session record for each zone "" has resolved to.
// A zone is known once its offset table is built.
// It stays unknown if tzdb rejects the timezone, or if there is no tzdb.
struct zone_record {
  std::string name; // The zone, e.g. "Europe/London"
  offset_table table; // Built from tzdb when it's resolved
  zone_record* next = nullptr; // Registry list
  bool invalid = false; // Rejected by the database
  bool session = false; // Is this a session timezone?

  bool known() const noexcept {
    return !table.offsets.empty();
  }

  const char* label() const noexcept {
    return session ? "" : name.c_str();
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
  const zone_record* find_or_add(std::string_view name) noexcept {
    if (name == "UTC") {
      return nullptr;
    }
    if (name.empty()) {
      return session_zone();
    }
    if (const zone_record* zone = find(name)) {
      return zone;
    }
    return add(name, /*session=*/ false);
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
      if (!zone->session && zone->name == name) {
        return zone;
      }
    }
    return nullptr;
  }

  const zone_record* find_session(std::string_view name) const noexcept {
    for (const zone_record* zone = head.load(std::memory_order_acquire); zone; zone = zone->next) {
      if (zone->session && zone->name == name) {
        return zone;
      }
    }
    return nullptr;
  }

  // Creates and publishes the record for `name`, unless another thread got there first
  const zone_record* add(std::string_view name, bool session) noexcept;
  const zone_record* session_zone() noexcept;
  void resolve(zone_record& zone) noexcept;
  bool ensure_db();
  static std::string find_sys_timezone();
};

// UTC offset in force at `tp` (0 for UTC), plus its abbreviation if `abbrev` is given. False for an unknown zone or if the lookup fails
inline bool utc_offset(const zone_record* zone, std::chrono::sys_seconds tp, std::chrono::seconds& out, const char** abbrev = nullptr) noexcept {
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
        *abbrev = zone->table.abbrevs[zone->table.abbrev_ids[i]].c_str();
      }
      return true;
    }
  } catch (...) {
    return false;
  }
  return false;
}

inline bool local_to_sys(const zone_record* zone, tz::local_seconds tp, std::chrono::sys_seconds& out,
                         roll on_impossible, roll on_ambiguous, bool& at_boundary) noexcept {
  at_boundary = false;
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
  int64_t shift = table.cycle_shift(local);
  local -= shift;

  int lo = table.period(local - 86400);
  int hi = table.period(local + 86400);

  int64_t utc_instant;

  if (lo >= 0 && hi >= 0) [[likely]] {
    int matches = 0; // 1 = unique, 0 = gap (non-existent), 2 = overlap (ambiguous)
    int64_t earliest = 0;
    int64_t latest = 0;
    int latest_i = 0; // Period whose offset gives latest instant showing this reading
    for (int i = lo; i <= hi; ++i) {
      utc_instant = local - table.offsets[i];
      if (utc_instant >= table.starts[i] && utc_instant < table.starts[i + 1]) {
        if (matches == 0) {
          earliest = utc_instant;
        }
        latest = utc_instant;
        latest_i = i;
        ++matches;
      }
    }
    // Most likely case is there are no ambiguous or non-existent results
    if (matches == 1) [[likely]] {
      out = std::chrono::sys_seconds{std::chrono::seconds{earliest + shift}};
      return true;
    }

    // Overlap (ambiguous)
    if (matches > 1) {
      on_ambiguous = resolve_roll(on_ambiguous, /*moving_forward=*/ true);
      if (on_ambiguous == roll::none) {
        return false;
      }
      utc_instant = earliest; // roll::backward
      if (on_ambiguous == roll::forward) {
        utc_instant = latest;
      } else if (on_ambiguous == roll::boundary) {
        utc_instant = table.starts[latest_i]; // Transition boundary, when the clocks went back
        at_boundary = true;
      }
      out = std::chrono::sys_seconds{std::chrono::seconds{utc_instant + shift}};
      return true;
    }
    // Gap (nonexistent): the reading is past the end of period i - 1 but before the start of period i
    on_impossible = resolve_roll(on_impossible, /*moving_forward=*/ true);
    if (on_impossible == roll::none) {
      return false;
    }
    
    for (int i = lo + 1; i <= hi; ++i) {
      int64_t transition_boundary = table.starts[i];
      int32_t prev_offset = table.offsets[i - 1]; // Before the transition
      int32_t curr_offset = table.offsets[i];     // After the transition
      // Too late for the old offset and too early for the new one
      bool in_gap = local - prev_offset >= transition_boundary && local - curr_offset < transition_boundary;
      if (in_gap) {
        utc_instant = transition_boundary; // roll::boundary
        if (on_impossible == roll::forward) {
          utc_instant = local - prev_offset; // Shift forward by the gap
        } else if (on_impossible == roll::backward) {
          utc_instant = local - curr_offset; // Shift backward by the gap
        } else {
          at_boundary = true;
        }
        out = std::chrono::sys_seconds{std::chrono::seconds{utc_instant + shift}};
        return true;
      }
    }
  }
  return false;
}

}

}

#ifdef CPPALLY_COMPILE_ONCE
#include <cppally/time/zone_impl.h>
#endif

#endif

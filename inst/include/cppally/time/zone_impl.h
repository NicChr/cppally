#ifndef CPPALLY_ZONE_IMPL_H
#define CPPALLY_ZONE_IMPL_H

// The out-of-line half of zone.h, compiled once per package: only in the translation unit that defines
// CPPALLY_COMPILE_ONCE (the generated cppally.cpp), so tz.h is parsed there and nowhere else.
// Everything here is non-inline and attribute_hidden, so each package keeps its own copy
// Define CPPALLY_WITHOUT_TZDB if you don't want tz.h and tzdata.h. If defined, only UTC date-times work everywhere and dates are unaffected.

#include <cppally/time/zone.h>
#ifndef CPPALLY_WITHOUT_TZDB
#include <cppally/time/tz.h>
#include <cppally/time/tzdata.h>
#endif
#include <cppally/r_sym.h>
#include <cppally/scalar/r_str.h>
#include <cppally/r_function.h>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>

namespace cppally {

namespace internal {

#ifndef CPPALLY_WITHOUT_TZDB

namespace tz {

// The database parsed from the embedded text, leaked deliberately. Set by zone_registry::ensure_db()
attribute_hidden tzdb*& tzdb_ptr() noexcept {
  static tzdb* p = nullptr;
  return p;
}

inline tzdb& get_tzdb() noexcept {
  return *tzdb_ptr();
}

}

// offset_table

offset_table::offset_table(const tz::time_zone& zone) {

  namespace chrono = std::chrono;

  chrono::sys_seconds end{chrono::sys_days{chrono::year{horizon_year} / chrono::January / 1}};

  // Tabulate 400 years from a year after the rules stop changing (margin for local_to_sys's +-1 day lookups)
  const chrono::year steady = zone.steady_year();
  if (steady != chrono::year::max()) {
    const chrono::sys_seconds cycle_start{chrono::sys_days{(steady + chrono::years{1}) / chrono::January / 1}};
    cycle_end = cycle_start.time_since_epoch().count() + cycle_seconds;
    end = std::max(end, chrono::sys_seconds{chrono::seconds{cycle_end + 2 * 86400}});
  }

  chrono::sys_seconds t{chrono::sys_days{chrono::year::min() / chrono::January / 1}};

  while (true) {

    tz::sys_info info = zone.get_info(t);
    int32_t offset = static_cast<int32_t>(info.offset.count());
    uint16_t id = abbrev_id(info.abbrev);

    // When offset changes (e.g. DST boundary)
    if (offsets.empty() || offset != offsets.back() || id != abbrev_ids.back()) {
      starts.push_back(offsets.empty() ? std::numeric_limits<int64_t>::min() : t.time_since_epoch().count());
      offsets.push_back(offset);
      abbrev_ids.push_back(id);
    }

    if (info.end <= t || info.end >= end) {
      starts.push_back(info.end.time_since_epoch().count());
      break;
    }
    t = info.end;
  }

  // A fixed-offset final zonelet lasts indefinitely.
  // It assumes that tzdata lists no changes beyond the horizon_year (currently 2100)
  if (steady == chrono::year::max()) {
    starts.back() = std::numeric_limits<int64_t>::max();
  }

  const size_t n = offsets.size();

  // If > 1 offset (e.g. a zone that observes DST will have at least 2)
  if (n > 1) {
    const int64_t first = starts[1];
    buckets.resize(static_cast<size_t>((starts[n - 1] - first) >> bucket_shift) + 1);
    size_t i = 1;
    for (size_t k = 0; k < buckets.size(); ++k) {
      const int64_t bucket_start = first + (static_cast<int64_t>(k) << bucket_shift);
      while (i + 1 < n && starts[i + 1] <= bucket_start) {
        ++i;
      }
      buckets[k] = static_cast<uint16_t>(i);
    }
  }
}

uint16_t offset_table::abbrev_id(const std::string& abbrev) {
  for (size_t i = 0; i < abbrevs.size(); ++i) {
    if (abbrevs[i] == abbrev) {
      return static_cast<uint16_t>(i);
    }
  }
  abbrevs.push_back(abbrev);
  return static_cast<uint16_t>(abbrevs.size() - 1);
}

#endif

// zone_registry

// Needed to prevent multi-threaded tzdb load and R C API calls. One per dll, never destroyed
attribute_hidden std::mutex& zone_registry_mutex() {
  static std::mutex& m = *new std::mutex;
  return m;
}

CPPALLY_NOINLINE const zone_record* zone_registry::add(std::string_view name, bool session) {
  std::lock_guard<std::mutex> lock(zone_registry_mutex());
  if (const zone_record* zone = session ? find_session(name) : find(name)) {
    return zone;
  }
  auto zone = std::make_unique<zone_record>();
  zone->name = name;
  zone->session = session;
  resolve(*zone);
  zone->next = head.load(std::memory_order_relaxed);
  head.store(zone.get(), std::memory_order_release);
  return zone.release();
}

// TZ if environment variable TZ is set, otherwise base::Sys.timezone() (cached)
CPPALLY_NOINLINE const zone_record* zone_registry::session_zone() {
  std::string_view session_name;
  const char* tz_env = std::getenv("TZ");
  if (tz_env && tz_env[0] != '\0') {
    session_name = tz_env;
  } else {

    // Cache the Sys.timezone() fallback if needed
    if (!session_cached.load(std::memory_order_acquire) && !OMP_IN_PARALLEL) {
      cache_session_zone();
    }
    // If cached use it
    if (session_cached.load(std::memory_order_acquire)) {
      session_name = sys_timezone;
    }
  }
  // Remove ":" prefix if it exists
  if (!session_name.empty() && session_name.front() == ':') {
    session_name.remove_prefix(1);
  }
  // Find the zone_record using session timezone name
  if (const zone_record* zone = find_session(session_name)) {
    return zone;
  }
  return add(session_name, /*session=*/ true);
}

// Sys.timezone() returns TZ (uncached) when it's set, so wait until TZ is unset to get the OS zone
void zone_registry::cache_session_zone() {
  const char* tz_env = std::getenv("TZ");
  // Avoid re-cache when:
  // session timezone already cached or
  // TZ is a non-empty string
  if (session_cached.load(std::memory_order_acquire) || (tz_env && tz_env[0] != '\0')) {
    return;
  }
  std::string zone = find_sys_timezone(); // base::Sys.timezone()
  std::lock_guard<std::mutex> lock(zone_registry_mutex());
  if (!session_cached.load(std::memory_order_relaxed)) {
    sys_timezone = std::move(zone);
    session_cached.store(true, std::memory_order_release);
  }
}

// Resolves a new, unpublished `zone` against the database, leaving it invalid on any failure. Call with the mutex held
void zone_registry::resolve(zone_record& zone) noexcept {
#ifdef CPPALLY_WITHOUT_TZDB
  // UTC aliases don't need tzdb, just a period with offset 0
  if (zone.name == "UTC" || zone.name == "Etc/UTC" || zone.name == "GMT" || zone.name == "Etc/GMT") {
    try {
      offset_table utc;
      utc.starts = {std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()};
      utc.offsets = {0};
      utc.abbrev_ids = {0};
      utc.abbrevs = {zone.name.ends_with("GMT") ? "GMT" : "UTC"};
      zone.table = std::move(utc);
    } catch (...) {}
  }
#else
  zone.invalid = true;
  try {
    if (zone.name.empty() || !ensure_db()) {
      return;
    }
    if (const tz::time_zone* resolved = tz::find_zone(zone.name)) {
      zone.table = offset_table(*resolved);
      zone.invalid = false;
    }
  } catch (...) {}
#endif
}

#ifndef CPPALLY_WITHOUT_TZDB

// Parses the embedded database on first use. Call with the mutex held
bool zone_registry::ensure_db() {
  if (tz::tzdb_ptr()) {
    return true;
  }
  if (db_failed) {
    return false;
  }
  try {
    tz::tzdb_ptr() = tz::init_tzdb(tz::tzdata_chunks).release();
    return true;
  } catch (...) {
    db_failed = true;
    return false;
  }
}

#endif

// Sys.timezone(), "" if NA
std::string zone_registry::find_sys_timezone() {

  r_str zone = r_str::na();
  r_sexp r_tzone = r_function(cached_sym<"Sys.timezone">(), env::base_env)();

  // zone_impl.h cannot include r_vector.h - so we have to use R C API vector access
  if (TYPEOF(r_tzone) == STRSXP && Rf_xlength(r_tzone) > 0) {
    zone = r_str(STRING_ELT(r_tzone, 0));
  }

  if (zone.is_na()){
    return std::string();
  }
  return std::string(zone.cpp_str());
}

}

}

#endif

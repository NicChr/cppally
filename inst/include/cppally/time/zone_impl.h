#ifndef CPPALLY_ZONE_IMPL_H
#define CPPALLY_ZONE_IMPL_H

// The out-of-line half of zone.h, compiled once per package: only in the translation unit that defines
// CPPALLY_COMPILE_ONCE (the generated cppally.cpp), so tz.h is parsed there and nowhere else.
// Everything here is non-inline and attribute_hidden, so each package keeps its own copy

#include <cppally/time/zone.h>
#include <cppally/time/tz.h>
#include <cppally/time/tzdata.h>
#include <cppally/r_sym.h>
#include <cppally/r_function.h>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>

namespace cppally {

namespace internal {

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

// First element of character vector, NA if no elements
inline r_str first_str(SEXP x) {
  if (TYPEOF(x) == STRSXP && Rf_xlength(x) > 0) {
    return r_str(STRING_ELT(x, 0), internal::no_checks_tag{});
  }
  return r_str::na();
}

// offset_table

offset_table::offset_table(const tz::time_zone& zone) {

  namespace chrono = std::chrono;

  const chrono::sys_seconds horizon{chrono::sys_days{chrono::year{horizon_year} / chrono::January / 1}};
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

    if (info.end <= t || info.end >= horizon) {
      starts.push_back(info.end.time_since_epoch().count());
      break;
    }
    t = info.end;
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

// zone_registry

// Needed to prevent multi-threaded tzdb load and R C API calls. One per dll, never destroyed
attribute_hidden std::mutex& zone_registry_mutex() {
  static std::mutex& m = *new std::mutex;
  return m;
}

CPPALLY_NOINLINE const zone_record* zone_registry::add(std::string_view name) {
  // Only the session zone ("") needs R, named zones resolve from the embedded database
  if (name.empty() && !OMP_IN_PARALLEL && needs_r_components()) {
    cache_r_components();
  }
  std::lock_guard<std::mutex> lock(zone_registry_mutex());
  if (const zone_record* zone = find(name)) {
    return zone;
  }
  auto zone = std::make_unique<zone_record>();
  zone->name = name;
  resolve(*zone);
  zone->next = head.load(std::memory_order_relaxed);
  head.store(zone.get(), std::memory_order_release);
  return zone.release();
}

void zone_registry::cache_r_components() {
  std::string session = find_session_zone_name();
  std::lock_guard<std::mutex> lock(zone_registry_mutex());
  session_name = std::move(session);
  r_components_done = true;
}

void zone_registry::update_session() {
  std::string session = find_session_zone_name();
  std::lock_guard<std::mutex> lock(zone_registry_mutex());
  session_name = std::move(session);
  for (zone_record* zone = head.load(std::memory_order_relaxed); zone; zone = zone->next) {
    if (zone->name.empty()) {
      resolve(*zone);
    }
  }
}

bool zone_registry::needs_r_components() {
  std::lock_guard<std::mutex> lock(zone_registry_mutex());
  return !r_components_done;
}

// Resolves `zone` against the database, leaving it unknown on any failure. Call with the mutex held
void zone_registry::resolve(zone_record& zone) noexcept {
  zone.tzdb_zone = nullptr;
  zone.table = offset_table();
  try {
    const std::string& zone_name = zone.name.empty() ? session_name : zone.name;
    if (zone_name.empty() || !ensure_db()) {
      return;
    }
    const tz::time_zone* resolved = tz::find_zone(zone_name);
    if (resolved) {
      zone.table = offset_table(*resolved);
      zone.tzdb_zone = resolved;
    }
  } catch (...) {
    zone.tzdb_zone = nullptr;
    zone.table = offset_table();
  }
}

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

// TZ if set, else Sys.timezone() (which returns its cache before checking TZ), ":" prefix removed
std::string zone_registry::find_session_zone_name() {

  std::string name;
  const char* tz_env = std::getenv("TZ");

  if (tz_env && tz_env[0] != '\0'){
    name = tz_env;
  } else {
    r_str zone = first_str(r_function(cached_sym<"Sys.timezone">(), env::base_env)());
    if (zone.is_na()){
      return std::string();
    }
    name = zone.cpp_str();
  }

  if (!name.empty() && name.front() == ':'){
    name.erase(0, 1);
  }
  return name;
}

// Lookups beyond a zone's offset_table

CPPALLY_NOINLINE bool tzdb_utc_offset(const zone_record* zone, std::chrono::sys_seconds tp, std::chrono::seconds& out, std::string* abbrev) noexcept {
  try {
    tz::sys_info info = zone->tzdb_zone->get_info(tp);
    out = info.offset;
    if (abbrev) {
      *abbrev = std::move(info.abbrev);
    }
    return true;
  } catch (...) {
    return false;
  }
}

CPPALLY_NOINLINE bool tzdb_local_to_sys(const zone_record* zone, tz::local_seconds tp, std::chrono::sys_seconds& out) noexcept {
  try {
    tz::local_info info = zone->tzdb_zone->get_info(tp);
    if (info.result != tz::local_info::unique) {
      return false;
    }
    out = std::chrono::sys_seconds{(tp - info.first.offset).time_since_epoch()};
    return true;
  } catch (...) {
    return false;
  }
}

}

}

#endif

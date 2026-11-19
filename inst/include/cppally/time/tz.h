#ifndef CPPALLY_TZ_H
#define CPPALLY_TZ_H

// IANA time zone database, adapted from Howard Hinnant's date library (tz.cpp, tz.h,
// tz_private.h) as vendored by the tzdb R package. Changes from the original: text
// database only, <chrono> in place of date.h, a string_view reader in place of
// iostreams, zones adjusted when located rather than via call_once, lookups of unknown
// zones return nullptr rather than throwing, zonelet's rule/save union replaced by plain
// members, and no remote/OS/Windows/leap-second code.
//
// The MIT License (MIT)
//
// Copyright (c) 2015, 2016, 2017 Howard Hinnant
// Copyright (c) 2015 Ville Voutilainen
// Copyright (c) 2016 Alexander Kormanovsky
// Copyright (c) 2016, 2017 Jiangang Zhuang
// Copyright (c) 2017 Nicolas Veloz Savino
// Copyright (c) 2017 Florian Dang
// Copyright (c) 2017 Aaron Bishop
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cppally/time/tz_fwd.h> // local_t, local_seconds
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace cppally {

namespace internal {

namespace tz {

namespace chrono = std::chrono;

struct sys_info {
    chrono::sys_seconds begin;
    chrono::sys_seconds end;
    chrono::seconds offset;
    chrono::minutes save;
    std::string abbrev;
};

struct local_info {
    enum {unique, nonexistent, ambiguous} result;
    sys_info first;
    sys_info second;
};

inline constexpr chrono::year min_year = chrono::year::min();
inline constexpr chrono::year max_year = chrono::year::max();
inline constexpr chrono::month_day min_day = chrono::January/1;
inline constexpr chrono::month_day max_day = chrono::December/31;

// Parsing

// Stand-in for the std::istringstream (failbit exceptions on) the original parses with
class line_reader {

    std::string_view s_;
    std::size_t pos_ = 0;

    [[noreturn]] static void fail() {
        throw std::runtime_error("malformed line in time zone database");
    }

public:

    explicit line_reader(std::string_view s) noexcept : s_(s) {}

    bool eof() const noexcept {
        return pos_ >= s_.size();
    }

    int peek() const noexcept {
        return eof() ? EOF : static_cast<unsigned char>(s_[pos_]);
    }

    int get() noexcept {
        return eof() ? EOF : static_cast<unsigned char>(s_[pos_++]);
    }

    void ws() noexcept {
        while (!eof() && std::isspace(static_cast<unsigned char>(s_[pos_]))) {
            ++pos_;
        }
    }

    // in >> word
    void read(std::string& out) {
        ws();
        if (eof()) {
            fail();
        }
        std::size_t start = pos_;
        while (!eof() && !std::isspace(static_cast<unsigned char>(s_[pos_]))) {
            ++pos_;
        }
        out.assign(s_.substr(start, pos_ - start));
    }

    // in >> int
    void read(int& out) {
        ws();
        bool negative = false;
        if (!eof() && (s_[pos_] == '-' || s_[pos_] == '+')) {
            negative = s_[pos_] == '-';
            ++pos_;
        }
        std::size_t digits_start = pos_;
        long long x = 0;
        while (!eof() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
            x = x * 10 + (s_[pos_] - '0');
            ++pos_;
        }
        if (pos_ == digits_start) {
            fail();
        }
        out = static_cast<int>(negative ? -x : x);
    }

    // in >> char
    void read(char& out) {
        ws();
        if (eof()) {
            fail();
        }
        out = s_[pos_++];
    }
};

inline char to_lower(char c) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

inline void to_lower(std::string& s) noexcept {
    for (auto& c : s) {
        c = to_lower(c);
    }
}

inline std::string get_alpha_word(line_reader& in) {
    in.ws();
    std::string s;
    while (!in.eof() && std::isalpha(in.peek())) {
        s.push_back(static_cast<char>(in.get()));
    }
    return s;
}

inline bool is_prefix_of(std::string_view key, std::string_view value) noexcept {
    const std::size_t size = std::min(key.size(), value.size());
    return key.substr(0, size) == value.substr(0, size);
}

inline unsigned parse_month(line_reader& in) {
    static constexpr std::string_view month_names[] = {
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december"
    };
    auto s = get_alpha_word(in);
    to_lower(s);
    for (unsigned m = 0; m < 12; ++m) {
        if (is_prefix_of(s, month_names[m])) {
            return m + 1;
        }
    }
    throw std::runtime_error("bad month name in time zone database: " + s);
}

inline unsigned parse_dow(line_reader& in) {
    static constexpr std::string_view dow_names[] = {
        "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
    };
    auto s = get_alpha_word(in);
    to_lower(s);
    for (unsigned d = 0; d < 7; ++d) {
        if (is_prefix_of(s, dow_names[d])) {
            return d;
        }
    }
    throw std::runtime_error("bad weekday name in time zone database: " + s);
}

inline chrono::seconds parse_unsigned_time(line_reader& in) {
    int x;
    in.read(x);
    auto r = chrono::seconds{chrono::hours{x}};
    if (!in.eof() && in.peek() == ':') {
        in.get();
        in.read(x);
        r += chrono::minutes{x};
        if (!in.eof() && in.peek() == ':') {
            in.get();
            in.read(x);
            r += chrono::seconds{x};
        }
    }
    return r;
}

inline chrono::seconds parse_signed_time(line_reader& in) {
    in.ws();
    auto sign = 1;
    if (in.peek() == '-') {
        sign = -1;
        in.get();
    } else if (in.peek() == '+') {
        in.get();
    }
    return sign * parse_unsigned_time(in);
}

// The clock a rule's AT time is read on: wall (local), standard or UTC
enum class rule_clock {utc, local, standard};

// MonthDayTime

class month_day_time {

    struct pair {
        chrono::month_day month_day_;
        chrono::weekday weekday_;
    };

    enum type {month_day, month_last_dow, lteq, gteq};

    type type_{month_day};

    union U {
        chrono::month_day month_day_;
        chrono::month_weekday_last month_weekday_last_;
        pair month_day_weekday_;

        U() : month_day_{chrono::January/1} {}

        U& operator=(const chrono::month_day& x) {
            month_day_ = x;
            return *this;
        }
        U& operator=(const chrono::month_weekday_last& x) {
            month_weekday_last_ = x;
            return *this;
        }
        U& operator=(const pair& x) {
            month_day_weekday_ = x;
            return *this;
        }
    } u;

    chrono::hours h_{0};
    chrono::minutes m_{0};
    chrono::seconds s_{0};
    rule_clock zone_{rule_clock::local};

public:

    month_day_time() = default;

    month_day_time(local_seconds tp, rule_clock zone) : zone_(zone) {
        const auto dp = chrono::floor<chrono::days>(tp);
        const auto hms = chrono::hh_mm_ss<chrono::seconds>(tp - dp);
        const auto ymd = chrono::year_month_day(chrono::sys_days{dp.time_since_epoch()});
        u = ymd.month() / ymd.day();
        h_ = hms.hours();
        m_ = hms.minutes();
        s_ = hms.seconds();
    }

    month_day_time(const chrono::month_day& md, rule_clock zone) : zone_(zone) {
        u = md;
    }

    chrono::day day() const {
        switch (type_) {
            case month_day: {
                return u.month_day_.day();
            }
            case month_last_dow: {
                return chrono::day{31};
            }
            case lteq:
            case gteq: {
                break;
            }
        }
        return u.month_day_weekday_.month_day_.day();
    }

    chrono::month month() const {
        switch (type_) {
            case month_day: {
                return u.month_day_.month();
            }
            case month_last_dow: {
                return u.month_weekday_last_.month();
            }
            case lteq:
            case gteq: {
                break;
            }
        }
        return u.month_day_weekday_.month_day_.month();
    }

    rule_clock zone() const {
        return zone_;
    }

    void canonicalize(chrono::year y) {
        switch (type_) {
            case month_day: {
                return;
            }
            case month_last_dow: {
                auto const ymd = chrono::year_month_day(chrono::sys_days(y/u.month_weekday_last_));
                u.month_day_ = ymd.month()/ymd.day();
                type_ = month_day;
                return;
            }
            case lteq: {
                auto const x = y/u.month_day_weekday_.month_day_;
                auto const wd1 = chrono::weekday(static_cast<chrono::sys_days>(x));
                auto const wd0 = u.month_day_weekday_.weekday_;
                auto const ymd = chrono::year_month_day(chrono::sys_days(x) - (wd1-wd0));
                u.month_day_ = ymd.month()/ymd.day();
                type_ = month_day;
                return;
            }
            case gteq: {
                auto const x = y/u.month_day_weekday_.month_day_;
                auto const wd1 = u.month_day_weekday_.weekday_;
                auto const wd0 = chrono::weekday(static_cast<chrono::sys_days>(x));
                auto const ymd = chrono::year_month_day(chrono::sys_days(x) + (wd1-wd0));
                u.month_day_ = ymd.month()/ymd.day();
                type_ = month_day;
                return;
            }
        }
    }

    chrono::sys_seconds to_sys(chrono::year y, chrono::seconds offset, chrono::seconds save) const {
        auto until_utc = to_time_point(y);
        if (zone_ == rule_clock::standard) {
            until_utc -= offset;
        } else if (zone_ == rule_clock::local) {
            until_utc -= offset + save;
        }
        return until_utc;
    }

    chrono::sys_days to_sys_days(chrono::year y) const {
        switch (type_) {
            case month_day: {
                return chrono::sys_days(y/u.month_day_);
            }
            case month_last_dow: {
                return chrono::sys_days(y/u.month_weekday_last_);
            }
            case lteq: {
                auto const x = y/u.month_day_weekday_.month_day_;
                auto const wd1 = chrono::weekday(static_cast<chrono::sys_days>(x));
                auto const wd0 = u.month_day_weekday_.weekday_;
                return chrono::sys_days(x) - (wd1-wd0);
            }
            case gteq: {
                break;
            }
        }
        auto const x = y/u.month_day_weekday_.month_day_;
        auto const wd1 = u.month_day_weekday_.weekday_;
        auto const wd0 = chrono::weekday(static_cast<chrono::sys_days>(x));
        return chrono::sys_days(x) + (wd1-wd0);
    }

    chrono::sys_seconds to_time_point(chrono::year y) const {
        // Add seconds first to promote to largest rep early to prevent overflow
        return to_sys_days(y) + s_ + h_ + m_;
    }

    int compare(chrono::year y, const month_day_time& x, chrono::year yx,
                chrono::seconds offset, chrono::minutes prev_save) const {
        if (zone_ != x.zone_) {
            auto dp0 = to_sys_days(y);
            auto dp1 = x.to_sys_days(yx);
            if (std::abs((dp0-dp1).count()) > 1) {
                return dp0 < dp1 ? -1 : 1;
            }
            if (zone_ == rule_clock::local) {
                auto tp0 = to_time_point(y) - prev_save;
                if (x.zone_ == rule_clock::utc) {
                    tp0 -= offset;
                }
                auto tp1 = x.to_time_point(yx);
                return tp0 < tp1 ? -1 : tp0 == tp1 ? 0 : 1;
            } else if (zone_ == rule_clock::standard) {
                auto tp0 = to_time_point(y);
                auto tp1 = x.to_time_point(yx);
                if (x.zone_ == rule_clock::local) {
                    tp1 -= prev_save;
                } else {
                    tp0 -= offset;
                }
                return tp0 < tp1 ? -1 : tp0 == tp1 ? 0 : 1;
            }
            // zone_ == rule_clock::utc
            auto tp0 = to_time_point(y);
            auto tp1 = x.to_time_point(yx);
            if (x.zone_ == rule_clock::local) {
                tp1 -= offset + prev_save;
            } else {
                tp1 -= offset;
            }
            return tp0 < tp1 ? -1 : tp0 == tp1 ? 0 : 1;
        }
        auto const t0 = to_time_point(y);
        auto const t1 = x.to_time_point(yx);
        return t0 < t1 ? -1 : t0 == t1 ? 0 : 1;
    }

    // is >> x
    friend void read(line_reader& is, month_day_time& x) {
        x = month_day_time{};
        is.ws();
        if (is.eof() || is.peek() == '#') {
            return;
        }
        auto m = parse_month(is);
        is.ws();
        if (!is.eof() && is.peek() != '#') {
            if (to_lower(static_cast<char>(is.peek())) == 'l') {
                for (int i = 0; i < 4; ++i) {
                    is.get();
                }
                auto dow = parse_dow(is);
                x.type_ = month_last_dow;
                x.u = chrono::month(m)/chrono::weekday(dow)[chrono::last];
            } else if (std::isalpha(is.peek())) {
                auto dow = parse_dow(is);
                char c{};
                is.read(c);
                if (c == '<' || c == '>') {
                    char c2{};
                    is.read(c2);
                    if (c2 != '=') {
                        throw std::runtime_error(std::string("bad operator in time zone database: ") + c + c2);
                    }
                    int d;
                    is.read(d);
                    if (d < 1 || d > 31) {
                        throw std::runtime_error(std::string("bad operator in time zone database: ") + c + c2 + std::to_string(d));
                    }
                    x.type_ = c == '<' ? lteq : gteq;
                    x.u = pair{chrono::month(m)/d, chrono::weekday(dow)};
                } else {
                    throw std::runtime_error(std::string("bad operator in time zone database: ") + c);
                }
            } else {
                int d;
                is.read(d);
                if (d < 1 || d > 31) {
                    throw std::runtime_error("bad day of month in time zone database: " + std::to_string(d));
                }
                x.type_ = month_day;
                x.u = chrono::month(m)/d;
            }
            is.ws();
            if (!is.eof() && is.peek() != '#') {
                int t;
                is.read(t);
                x.h_ = chrono::hours{t};
                if (!is.eof() && is.peek() == ':') {
                    is.get();
                    is.read(t);
                    x.m_ = chrono::minutes{t};
                    if (!is.eof() && is.peek() == ':') {
                        is.get();
                        is.read(t);
                        x.s_ = chrono::seconds{t};
                    }
                }
                if (!is.eof() && std::isalpha(is.peek())) {
                    char c;
                    is.read(c);
                    switch (c) {
                        case 's': {
                            x.zone_ = rule_clock::standard;
                            break;
                        }
                        case 'u': {
                            x.zone_ = rule_clock::utc;
                            break;
                        }
                    }
                }
            }
        } else {
            x.u = chrono::month{m}/1;
        }
    }
};

// Input is almost sorted in update_tzdata.R so insertion_sort() has little to sort
template <typename T>
void insertion_sort(T* x, std::size_t n) {
    for (std::size_t i = 1; i < n; ++i) {
        T curr = std::move(x[i]);
        std::size_t j = i;
        for (; j > 0 && curr < x[j - 1]; --j) {
            x[j] = std::move(x[j - 1]);
        }
        x[j] = std::move(curr);
    }
}

// Rule

// A rule specifies one or more set of datetimes without using an offset.
// Multiple dates are specified with multiple years. The years in effect
// go from starting_year_ to ending_year_, inclusive. starting_year_ <=
// ending_year_. save_ is in effect for times from the specified time
// onward, including the specified time. When the specified time is
// local, it uses the save_ from the chronologically previous rule, or if
// there is none, 0.
class rule {

    std::string name_;
    chrono::year starting_year_{0};
    chrono::year ending_year_{0};
    month_day_time starting_at_;
    chrono::minutes save_{0};
    std::string abbrev_;

public:

    rule() = default;

    explicit rule(std::string_view s) {
        line_reader in(s);
        std::string word;
        in.read(word);
        in.read(name_);
        int x;
        in.ws();
        if (std::isalpha(in.peek())) {
            in.read(word);
            if (word == "min") {
                starting_year_ = chrono::year::min();
            } else {
                throw std::runtime_error("didn't find expected word in time zone database: " + word);
            }
        } else {
            in.read(x);
            starting_year_ = chrono::year{x};
        }
        in.ws();
        if (std::isalpha(in.peek())) {
            in.read(word);
            if (word == "only") {
                ending_year_ = starting_year_;
            } else if (word == "max") {
                ending_year_ = chrono::year::max();
            } else {
                throw std::runtime_error("didn't find expected word in time zone database: " + word);
            }
        } else {
            in.read(x);
            ending_year_ = chrono::year{x};
        }
        in.read(word);  // TYPE (always "-")
        read(in, starting_at_);
        save_ = chrono::duration_cast<chrono::minutes>(parse_signed_time(in));
        in.read(abbrev_);
        if (abbrev_ == "-") {
            abbrev_.clear();
        }
    }

    rule(const rule& r, chrono::year starting_year, chrono::year ending_year)
        : name_(r.name_)
        , starting_year_(starting_year)
        , ending_year_(ending_year)
        , starting_at_(r.starting_at_)
        , save_(r.save_)
        , abbrev_(r.abbrev_)
    {}

    const std::string& name() const {
        return name_;
    }
    const std::string& abbrev() const {
        return abbrev_;
    }
    const month_day_time& mdt() const {
        return starting_at_;
    }
    const chrono::year& starting_year() const {
        return starting_year_;
    }
    const chrono::year& ending_year() const {
        return ending_year_;
    }
    const chrono::minutes& save() const {
        return save_;
    }

    friend bool operator<(const rule& x, const rule& y) {
        auto const xm = x.month();
        auto const ym = y.month();
        if (std::tie(x.name_, x.starting_year_, xm, x.ending_year_) <
            std::tie(y.name_, y.starting_year_, ym, y.ending_year_)) {
            return true;
        }
        if (std::tie(x.name_, x.starting_year_, xm, x.ending_year_) >
            std::tie(y.name_, y.starting_year_, ym, y.ending_year_)) {
            return false;
        }
        return x.day() < y.day();
    }

    friend bool operator<(const rule& x, const chrono::year& y) {
        return x.ending_year_ < y;
    }

    friend bool operator<(const chrono::year& x, const rule& y) {
        return x < y.starting_year_;
    }

    friend bool operator<(const rule& x, const std::string& y) {
        return x.name_ < y;
    }

    friend bool operator<(const std::string& x, const rule& y) {
        return x < y.name_;
    }

    static void split_overlaps(std::vector<rule>& rules) {
        using difference_type = std::vector<rule>::iterator::difference_type;
        for (std::size_t i = 0; i < rules.size();) {
            auto e = static_cast<std::size_t>(std::upper_bound(
                rules.cbegin() + static_cast<difference_type>(i), rules.cend(), rules[i].name(),
                [](const std::string& nm, const rule& x) {
                    return nm < x.name();
                }) - rules.cbegin());
            split_overlaps(rules, i, e);
            auto first_rule = rules.begin() + static_cast<difference_type>(i);
            auto last_rule = rules.begin() + static_cast<difference_type>(e);
            auto t = std::lower_bound(first_rule, last_rule, min_year);
            if (t > first_rule + 1) {
                if (t == last_rule || t->starting_year() >= min_year) {
                    --t;
                }
                auto d = static_cast<std::size_t>(t - first_rule);
                rules.erase(first_rule, t);
                e -= d;
            }
            first_rule = rules.begin() + static_cast<difference_type>(i);
            last_rule = rules.begin() + static_cast<difference_type>(e);
            t = std::upper_bound(first_rule, last_rule, max_year);
            if (t != last_rule) {
                auto d = static_cast<std::size_t>(last_rule - t);
                rules.erase(t, last_rule);
                e -= d;
            }
            i = e;
        }
        rules.shrink_to_fit();
    }

private:

    chrono::day day() const {
        return starting_at_.day();
    }

    chrono::month month() const {
        return starting_at_.month();
    }

    static bool overlaps(const rule& x, const rule& y) {
        // assume x.starting_year_ <= y.starting_year_;
        if (y.starting_year_ > x.ending_year_) {
            return false;
        }
        return !(x.starting_year_ == y.starting_year_ && x.ending_year_ == y.ending_year_);
    }

    static void split(std::vector<rule>& rules, std::size_t i, std::size_t k, std::size_t& e) {
        using difference_type = std::vector<rule>::iterator::difference_type;
        // rules[i].starting_year_ <= rules[k].starting_year_ &&
        //     rules[i].ending_year_ >= rules[k].starting_year_ &&
        //     (rules[i].starting_year_ != rules[k].starting_year_ ||
        //      rules[i].ending_year_ != rules[k].ending_year_)
        if (rules[i].starting_year_ == rules[k].starting_year_) {
            if (rules[k].ending_year_ < rules[i].ending_year_) {
                rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                             rule(rules[i], rules[k].ending_year_ + chrono::years{1},
                                  std::move(rules[i].ending_year_)));
                ++e;
                rules[i].ending_year_ = rules[k].ending_year_;
            } else { // rules[k].ending_year_ > rules[i].ending_year_
                rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                             rule(rules[k], rules[i].ending_year_ + chrono::years{1},
                                  std::move(rules[k].ending_year_)));
                ++e;
                rules[k].ending_year_ = rules[i].ending_year_;
            }
        } else { // rules[i].starting_year_ < rules[k].starting_year_
            if (rules[k].ending_year_ < rules[i].ending_year_) {
                rules.insert(rules.begin() + static_cast<difference_type>(k),
                             rule(rules[i], rules[k].starting_year_, rules[k].ending_year_));
                ++k;
                rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                             rule(rules[i], rules[k].ending_year_ + chrono::years{1},
                                  std::move(rules[i].ending_year_)));
                rules[i].ending_year_ = rules[k].starting_year_ - chrono::years{1};
                e += 2;
            } else if (rules[k].ending_year_ > rules[i].ending_year_) {
                rules.insert(rules.begin() + static_cast<difference_type>(k),
                             rule(rules[i], rules[k].starting_year_, rules[i].ending_year_));
                ++k;
                rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                             rule(rules[k], rules[i].ending_year_ + chrono::years{1},
                                  std::move(rules[k].ending_year_)));
                e += 2;
                rules[k].ending_year_ = std::move(rules[i].ending_year_);
                rules[i].ending_year_ = rules[k].starting_year_ - chrono::years{1};
            } else { // rules[k].ending_year_ == rules[i].ending_year_
                rules.insert(rules.begin() + static_cast<difference_type>(k),
                             rule(rules[i], rules[k].starting_year_,
                                  std::move(rules[i].ending_year_)));
                ++k;
                ++e;
                rules[i].ending_year_ = rules[k].starting_year_ - chrono::years{1};
            }
        }
    }

    static void split_overlaps(std::vector<rule>& rules, std::size_t i, std::size_t& e) {
        auto j = i;
        for (; i + 1 < e; ++i) {
            for (auto k = i + 1; k < e; ++k) {
                if (overlaps(rules[i], rules[k])) {
                    split(rules, i, k, e);
                    insertion_sort(rules.data() + i, e - i);
                }
            }
        }
        for (; j < e; ++j) {
            if (rules[j].starting_year() == rules[j].ending_year()) {
                rules[j].starting_at_.canonicalize(rules[j].starting_year());
            }
        }
    }
};

// Rules of the loaded database, defined below tzdb
inline const std::vector<rule>& db_rules();

// Find the rule that comes chronologically before rule r. For multi-year rules,
// y specifies which rules in r. For single year rules, y is assumed to be equal
// to the year specified by r.
// Returns a pointer to the chronologically previous rule, and the year within
// that rule. If there is no previous rule, returns nullptr and year::min().
// Preconditions:
//     r->starting_year() <= y && y <= r->ending_year()
inline std::pair<const rule*, chrono::year> find_previous_rule(const rule* r, chrono::year y) {
    auto const& rules = db_rules();
    if (y == r->starting_year()) {
        if (r == &rules.front() || r->name() != r[-1].name()) {
            throw std::logic_error("find_previous_rule called with the first rule");
        }
        --r;
        if (y == r->starting_year()) {
            return {r, y};
        }
        return {r, r->ending_year()};
    }
    if (r == &rules.front() || r->name() != r[-1].name() ||
        r[-1].starting_year() < r->starting_year()) {
        while (r < &rules.back() && r->name() == r[1].name() &&
               r->starting_year() == r[1].starting_year()) {
            ++r;
        }
        return {r, --y};
    }
    --r;
    return {r, y};
}

// Find the rule that comes chronologically after rule r. For multi-year rules,
// y specifies which rules in r. For single year rules, y is assumed to be equal
// to the year specified by r.
// Returns a pointer to the chronologically next rule, and the year within
// that rule. If there is no next rule, return nullptr and year::max().
// Preconditions:
//     first <= r && r < last && r->starting_year() <= y && y <= r->ending_year()
//     [first, last) all have the same name
inline std::pair<const rule*, chrono::year> find_next_rule(const rule* first_rule, const rule* last_rule, const rule* r, chrono::year y) {
    if (y == r->ending_year()) {
        if (r == last_rule - 1) {
            return {nullptr, chrono::year::max()};
        }
        ++r;
        if (y == r->ending_year()) {
            return {r, y};
        }
        return {r, r->starting_year()};
    }
    if (r == last_rule - 1 || r->ending_year() < r[1].ending_year()) {
        while (r > first_rule && r->starting_year() == r[-1].starting_year()) {
            --r;
        }
        return {r, ++y};
    }
    ++r;
    return {r, y};
}

// As above but over all rules of the database, stopping where the name changes
inline std::pair<const rule*, chrono::year> find_next_rule(const rule* r, chrono::year y) {
    auto const& rules = db_rules();
    if (y == r->ending_year()) {
        if (r == &rules.back() || r->name() != r[1].name()) {
            return {nullptr, chrono::year::max()};
        }
        ++r;
        if (y == r->ending_year()) {
            return {r, y};
        }
        return {r, r->starting_year()};
    }
    if (r == &rules.back() || r->name() != r[1].name() ||
        r->ending_year() < r[1].ending_year()) {
        while (r > &rules.front() && r->name() == r[-1].name() &&
               r->starting_year() == r[-1].starting_year()) {
            --r;
        }
        return {r, ++y};
    }
    ++r;
    return {r, y};
}

inline const rule* find_first_std_rule(const std::pair<const rule*, const rule*>& eqr) {
    auto r = eqr.first;
    auto ry = r->starting_year();
    while (r->save() != chrono::minutes{0}) {
        std::tie(r, ry) = find_next_rule(eqr.first, eqr.second, r, ry);
        if (r == nullptr) {
            throw std::runtime_error("could not find standard offset in time zone rule " + eqr.first->name());
        }
    }
    return r;
}

inline std::pair<const rule*, chrono::year>
find_rule_for_zone(const std::pair<const rule*, const rule*>& eqr,
                   const chrono::year& y, const chrono::seconds& offset,
                   const month_day_time& mdt) {
    auto r = eqr.first;
    auto ry = r->starting_year();
    auto prev_save = chrono::minutes{0};
    auto prev_year = chrono::year::min();
    const rule* prev_rule = nullptr;
    while (r != nullptr) {
        if (mdt.compare(y, r->mdt(), ry, offset, prev_save) <= 0) {
            break;
        }
        prev_rule = r;
        prev_year = ry;
        prev_save = prev_rule->save();
        std::tie(r, ry) = find_next_rule(eqr.first, eqr.second, r, ry);
    }
    return {prev_rule, prev_year};
}

inline std::pair<const rule*, chrono::year>
find_rule_for_zone(const std::pair<const rule*, const rule*>& eqr,
                   const chrono::sys_seconds& tp_utc,
                   const local_seconds& tp_std,
                   const local_seconds& tp_loc) {
    auto r = eqr.first;
    auto ry = r->starting_year();
    auto prev_year = chrono::year::min();
    const rule* prev_rule = nullptr;
    while (r != nullptr) {
        bool found = false;
        switch (r->mdt().zone()) {
            case rule_clock::utc: {
                found = tp_utc < r->mdt().to_time_point(ry);
                break;
            }
            case rule_clock::standard: {
                found = chrono::sys_seconds{tp_std.time_since_epoch()} < r->mdt().to_time_point(ry);
                break;
            }
            case rule_clock::local: {
                found = chrono::sys_seconds{tp_loc.time_since_epoch()} < r->mdt().to_time_point(ry);
                break;
            }
        }
        if (found) {
            break;
        }
        prev_rule = r;
        prev_year = ry;
        std::tie(r, ry) = find_next_rule(eqr.first, eqr.second, r, ry);
    }
    return {prev_rule, prev_year};
}

inline sys_info find_rule(const std::pair<const rule*, chrono::year>& first_rule,
                          const std::pair<const rule*, chrono::year>& last_rule,
                          const chrono::year& y, const chrono::seconds& offset,
                          const month_day_time& mdt, const chrono::minutes& initial_save,
                          const std::string& initial_abbrev) {
    auto r = first_rule.first;
    auto ry = first_rule.second;
    sys_info x{chrono::sys_days(chrono::year::min()/min_day), chrono::sys_days(chrono::year::max()/max_day),
               chrono::seconds{0}, initial_save, initial_abbrev};
    while (r != nullptr) {
        auto tr = r->mdt().to_sys(ry, offset, x.save);
        auto tx = mdt.to_sys(y, offset, x.save);
        // Find last rule where tx >= tr
        if (tx <= tr || (r == last_rule.first && ry == last_rule.second)) {
            if (tx < tr && r == first_rule.first && ry == first_rule.second) {
                x.end = r->mdt().to_sys(ry, offset, x.save);
                break;
            }
            if (tx < tr) {
                std::tie(r, ry) = find_previous_rule(r, ry);  // can't return nullptr for r
            }
            // r != nullptr && tx >= tr (if tr were to be recomputed)
            auto prev_save = initial_save;
            if (!(r == first_rule.first && ry == first_rule.second)) {
                prev_save = find_previous_rule(r, ry).first->save();
            }
            x.begin = r->mdt().to_sys(ry, offset, prev_save);
            x.save = r->save();
            x.abbrev = r->abbrev();
            if (!(r == last_rule.first && ry == last_rule.second)) {
                std::tie(r, ry) = find_next_rule(r, ry);  // can't return nullptr for r
                x.end = r->mdt().to_sys(ry, offset, x.save);
            } else {
                x.end = chrono::sys_days(chrono::year::max()/max_day);
            }
            break;
        }
        x.save = r->save();
        std::tie(r, ry) = find_next_rule(r, ry);  // can't return nullptr for r
    }
    return x;
}

inline std::string format_abbrev(std::string format, const std::string& variable,
                                 chrono::seconds off, chrono::minutes save) {
    auto k = format.find("%s");
    if (k != std::string::npos) {
        format.replace(k, 2, variable);
    } else {
        k = format.find('/');
        if (k != std::string::npos) {
            if (save == chrono::minutes{0}) {
                format.erase(k);
            } else {
                format.erase(0, k+1);
            }
        } else {
            k = format.find("%z");
            if (k != std::string::npos) {
                std::string temp;
                if (off < chrono::seconds{0}) {
                    temp = '-';
                    off = -off;
                } else {
                    temp = '+';
                }
                auto h = chrono::floor<chrono::hours>(off);
                off -= h;
                if (h < chrono::hours{10}) {
                    temp += '0';
                }
                temp += std::to_string(h.count());
                if (off > chrono::seconds{0}) {
                    auto m = chrono::floor<chrono::minutes>(off);
                    off -= m;
                    if (m < chrono::minutes{10}) {
                        temp += '0';
                    }
                    temp += std::to_string(m.count());
                    if (off > chrono::seconds{0}) {
                        if (off < chrono::seconds{10}) {
                            temp += '0';
                        }
                        temp += std::to_string(off.count());
                    }
                }
                format.replace(k, 2, temp);
            }
        }
    }
    return format;
}

// zonelet

struct zonelet {

    enum tag {has_rule, has_save, is_empty};

    chrono::seconds gmtoff_;
    tag tag_ = has_rule;
    std::string rule_;        // has_rule
    chrono::minutes save_{0}; // has_save
    std::string format_;
    chrono::year until_year_{0};
    month_day_time until_date_;
    chrono::sys_seconds until_utc_;
    local_seconds until_std_;
    local_seconds until_loc_;
    chrono::minutes initial_save_{0};
    std::string initial_abbrev_;
    std::pair<const rule*, chrono::year> first_rule_{nullptr, chrono::year::min()};
    std::pair<const rule*, chrono::year> last_rule_{nullptr, chrono::year::max()};
};

// time_zone

class time_zone {

    std::string name_;
    std::vector<zonelet> zonelets_;
    bool adjusted_ = false;

public:

    time_zone(time_zone&&) = default;
    time_zone& operator=(time_zone&&) = default;

    explicit time_zone(std::string_view s) {
        line_reader in(s);
        std::string word;
        in.read(word);
        in.read(name_);
        parse_info(in);
    }

    const std::string& name() const noexcept {
        return name_;
    }

    // Continuation line of the zone
    void add(std::string_view s) {
        line_reader in(s);
        in.ws();
        if (!in.eof() && in.peek() != '#') {
            parse_info(in);
        }
    }

    // Must run before any get_info() call. Main thread only
    void adjust(const std::vector<rule>& rules) {
        if (!adjusted_) {
            adjust_infos(rules);
            adjusted_ = true;
        }
    }

    sys_info get_info(chrono::sys_seconds tp) const {
        return get_info_impl(tp, rule_clock::utc);
    }

    local_info get_info(local_seconds tp) const {
        local_info i{};
        i.first = get_info_impl(chrono::sys_seconds{tp.time_since_epoch()}, rule_clock::local);
        auto tps = chrono::sys_seconds{(tp - i.first.offset).time_since_epoch()};
        if (tps < i.first.begin) {
            i.second = std::move(i.first);
            i.first = get_info_impl(i.second.begin - chrono::seconds{1}, rule_clock::utc);
            i.result = local_info::nonexistent;
        } else if (i.first.end - tps <= chrono::days{1}) {
            i.second = get_info_impl(i.first.end, rule_clock::utc);
            tps = chrono::sys_seconds{(tp - i.second.offset).time_since_epoch()};
            if (tps >= i.second.begin) {
                i.result = local_info::ambiguous;
            } else {
                i.second = {};
            }
        }
        return i;
    }

    // First year the last zonelet and its rules are in force unchanged, max() if that zonelet has a fixed offset. Call after adjust()
    chrono::year steady_year() const {
        const zonelet& z = zonelets_.back();
        if (z.tag_ != zonelet::has_rule) {
            return chrono::year::max();
        }
        chrono::year y = zonelets_.size() > 1 ? zonelets_.end()[-2].until_year_ + chrono::years{1} : chrono::year::min();
        auto const& rules = db_rules();
        auto eqr = std::equal_range(rules.data(), rules.data() + rules.size(), z.rule_);
        for (const rule* r = eqr.first; r != eqr.second; ++r) {
            y = std::max(y, r->ending_year() == chrono::year::max() ? r->starting_year() : r->ending_year() + chrono::years{1});
        }
        return y;
    }

    friend bool operator<(const time_zone& x, const time_zone& y) noexcept {
        return x.name_ < y.name_;
    }

private:

    void parse_info(line_reader& in) {
        zonelets_.emplace_back();
        auto& zl = zonelets_.back();
        zl.gmtoff_ = parse_signed_time(in);
        in.read(zl.rule_);
        if (zl.rule_ == "-") {
            zl.rule_.clear();
        }
        in.read(zl.format_);
        if (!in.eof()) {
            in.ws();
        }
        if (in.eof() || in.peek() == '#') {
            zl.until_year_ = chrono::year::max();
            zl.until_date_ = month_day_time(max_day, rule_clock::utc);
        } else {
            int y;
            in.read(y);
            zl.until_year_ = chrono::year{y};
            read(in, zl.until_date_);
            zl.until_date_.canonicalize(zl.until_year_);
        }
        if ((zl.until_year_ < min_year) ||
                (zonelets_.size() > 1 && zonelets_.end()[-2].until_year_ > max_year)) {
            zonelets_.pop_back();
        }
    }

    void adjust_infos(const std::vector<rule>& rules) {
        const zonelet* prev_zonelet = nullptr;
        for (auto& z : zonelets_) {
            std::pair<const rule*, const rule*> eqr{};
            // Classify info as rule-based, has save, or neither
            if (!z.rule_.empty()) {
                // Find out if this zonelet has a rule or a save
                eqr = std::equal_range(rules.data(), rules.data() + rules.size(), z.rule_);
                if (eqr.first == eqr.second) {
                    // The rule doesn't exist. Assume this is a save
                    line_reader in(z.rule_);
                    z.save_ = chrono::duration_cast<chrono::minutes>(parse_signed_time(in));
                    z.rule_.clear();
                    z.tag_ = zonelet::has_save;
                }
            } else {
                // This zonelet has no rule and no save
                z.tag_ = zonelet::is_empty;
            }

            chrono::minutes final_save{0};
            if (z.tag_ == zonelet::has_save) {
                final_save = z.save_;
            } else if (z.tag_ == zonelet::has_rule) {
                z.last_rule_ = find_rule_for_zone(eqr, z.until_year_, z.gmtoff_, z.until_date_);
                if (z.last_rule_.first != nullptr) {
                    final_save = z.last_rule_.first->save();
                }
            }
            z.until_utc_ = z.until_date_.to_sys(z.until_year_, z.gmtoff_, final_save);
            z.until_std_ = local_seconds{z.until_utc_.time_since_epoch()} + z.gmtoff_;
            z.until_loc_ = z.until_std_ + final_save;

            if (z.tag_ == zonelet::has_rule) {
                if (prev_zonelet != nullptr) {
                    z.first_rule_ = find_rule_for_zone(eqr, prev_zonelet->until_utc_,
                                                            prev_zonelet->until_std_,
                                                            prev_zonelet->until_loc_);
                    if (z.first_rule_.first != nullptr) {
                        z.initial_save_ = z.first_rule_.first->save();
                        z.initial_abbrev_ = z.first_rule_.first->abbrev();
                        if (z.first_rule_ != z.last_rule_) {
                            z.first_rule_ = find_next_rule(eqr.first, eqr.second,
                                                           z.first_rule_.first,
                                                           z.first_rule_.second);
                        } else {
                            z.first_rule_ = std::make_pair(nullptr, chrono::year::min());
                            z.last_rule_ = std::make_pair(nullptr, chrono::year::max());
                        }
                    }
                }
                if (z.first_rule_.first == nullptr && z.last_rule_.first != nullptr) {
                    z.first_rule_ = std::make_pair(eqr.first, eqr.first->starting_year());
                    z.initial_abbrev_ = find_first_std_rule(eqr)->abbrev();
                }
            }
            prev_zonelet = &z;
        }
    }

    sys_info get_info_impl(chrono::sys_seconds tp, rule_clock kind) const {
        auto y = chrono::year_month_day(chrono::floor<chrono::days>(tp)).year();
        if (y < min_year || y > max_year) {
            throw std::runtime_error("the year " + std::to_string(static_cast<int>(y)) + " is out of range");
        }
        auto i = std::upper_bound(zonelets_.begin(), zonelets_.end(), tp,
            [kind](chrono::sys_seconds t, const zonelet& zl) {
                return kind == rule_clock::utc ? t < zl.until_utc_ :
                                                     t < chrono::sys_seconds{zl.until_loc_.time_since_epoch()};
            });

        sys_info r{};
        if (i != zonelets_.end()) {
            if (i->tag_ == zonelet::has_save) {
                if (i != zonelets_.begin()) {
                    r.begin = i[-1].until_utc_;
                } else {
                    r.begin = chrono::sys_days(chrono::year::min()/min_day);
                }
                r.end = i->until_utc_;
                r.offset = i->gmtoff_ + i->save_;
                r.save = i->save_;
            } else if (i->rule_.empty()) {
                if (i != zonelets_.begin()) {
                    r.begin = i[-1].until_utc_;
                } else {
                    r.begin = chrono::sys_days(chrono::year::min()/min_day);
                }
                r.end = i->until_utc_;
                r.offset = i->gmtoff_;
            } else {
                r = find_rule(i->first_rule_, i->last_rule_, y, i->gmtoff_,
                              month_day_time(local_seconds{tp.time_since_epoch()}, kind),
                              i->initial_save_, i->initial_abbrev_);
                r.offset = i->gmtoff_ + r.save;
                if (i != zonelets_.begin() && r.begin < i[-1].until_utc_) {
                    r.begin = i[-1].until_utc_;
                }
                if (r.end > i->until_utc_) {
                    r.end = i->until_utc_;
                }
            }
            r.abbrev = format_abbrev(i->format_, r.abbrev, r.offset, r.save);
        }
        return r;
    }
};

class time_zone_link {

    std::string name_;
    std::string target_;

public:

    explicit time_zone_link(std::string_view s) {
        line_reader in(s);
        std::string word;
        in.read(word);
        in.read(target_);
        in.read(name_);
    }

    const std::string& name() const noexcept {
        return name_;
    }

    const std::string& target() const noexcept {
        return target_;
    }

    friend bool operator<(const time_zone_link& x, const time_zone_link& y) noexcept {
        return x.name_ < y.name_;
    }
};

// Database

struct tzdb {

    std::string version;
    std::vector<time_zone> zones;
    std::vector<time_zone_link> links;
    std::vector<rule> rules;

    // nullptr if `tz_name` is neither a zone nor a link to one
    time_zone* locate_zone(std::string_view tz_name, int depth = 0) {
        // If a link-to-link chain exceeds this limit, give up
        if (depth > 10) {
            return nullptr;
        }
        auto zi = std::lower_bound(zones.begin(), zones.end(), tz_name,
            [](const time_zone& z, std::string_view nm) {
                return z.name() < nm;
            });
        if (zi == zones.end() || zi->name() != tz_name) {
            auto li = std::lower_bound(links.begin(), links.end(), tz_name,
                [](const time_zone_link& z, std::string_view nm) {
                    return z.name() < nm;
                });
            if (li != links.end() && li->name() == tz_name) {
                return locate_zone(li->target(), depth + 1);
            }
            return nullptr;
        }
        return &*zi;
    }
};

// Parses zic input text: the IANA source files joined into one string (see tzdata.h and data-raw/update_tzdata.R).
// Throws std::runtime_error on failure
[[gnu::cold]] inline std::unique_ptr<tzdb> init_tzdb(std::string_view text) {
    auto db = std::make_unique<tzdb>();
    constexpr std::string_view version_tag = "# version ";
    bool continue_zone = false;
    while (!text.empty()) {
        std::size_t eol = text.find('\n');
        std::string_view line = text.substr(0, eol);
        text = eol == std::string_view::npos ? std::string_view{} : text.substr(eol + 1);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            // "# version 2026d", as in IANA's own tzdata.zi
            if (db->version.empty() && line.substr(0, version_tag.size()) == version_tag) {
                db->version = std::string(line.substr(version_tag.size()));
            }
            continue;
        }
        line_reader in(line);
        in.ws();
        if (in.eof()) {
            continue;
        }
        std::string word;
        in.read(word);
        to_lower(word);
        if (is_prefix_of(word, "rule")) {
            db->rules.push_back(rule(line));
            continue_zone = false;
        } else if (is_prefix_of(word, "link")) {
            db->links.push_back(time_zone_link(line));
            continue_zone = false;
        } else if (is_prefix_of(word, "leap")) {
            continue_zone = false;
        } else if (is_prefix_of(word, "zone")) {
            db->zones.push_back(time_zone(line));
            continue_zone = true;
        } else if ((line[0] == '\t' || line[0] == ' ') && continue_zone) {
            db->zones.back().add(line);
        }
    }
    insertion_sort(db->rules.data(), db->rules.size());
    rule::split_overlaps(db->rules);
    // update_tzdata.R emits zones and links sorted by name
    if (!std::is_sorted(db->zones.begin(), db->zones.end()) || !std::is_sorted(db->links.begin(), db->links.end())) {
        throw std::runtime_error("tzdata zones and links are not sorted by name");
    }
    db->zones.shrink_to_fit();
    db->links.shrink_to_fit();
    return db;
}

// The loaded database, defined in zone_impl.h, which keeps one per shared library
inline tzdb& get_tzdb() noexcept;

inline const std::vector<rule>& db_rules() {
    return get_tzdb().rules;
}

// Locates `name` (following links) and readies it for get_info(); nullptr if unknown.
// Requires a loaded database. Not thread-safe, zone_registry serialises calls
[[gnu::cold]] inline const time_zone* find_zone(std::string_view name) {
    time_zone* zone = get_tzdb().locate_zone(name);
    if (zone) {
        zone->adjust(get_tzdb().rules);
    }
    return zone;
}

}

}

}

#endif

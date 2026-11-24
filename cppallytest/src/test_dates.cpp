#include "cppallytest.h"
using namespace cppally;

// The reference date used is 2024-03-15, which is a Friday and in a leap year
// The reference date-time used is 2024-03-15 13:45:30 UTC

constexpr r_date ref_day = r_date(2024, 3, 15); // 2024-03-15
constexpr r_psxct ref_sec = r_psxct(2024, 3, 15, 13, 45, 30.0); // 2024-03-15 13:45:30 UTC

[[cppally::register]]
void test_date_accessors(){

    r_date x(ref_day); // 2024-03-15

    expect_identical(x.days_since_epoch(), r_dbl(19797.0), "days_since_epoch");
    expect_identical(x.year(), r_int(2024), "year");
    expect_identical(x.month(), r_int(3), "month");
    expect_identical(x.day(), r_int(15), "day");
    expect_identical(x.yday(), r_int(75), "yday");
    expect_identical(x.week(), r_int(11), "week");
    expect_identical(x.iso_week(), r_int(11), "iso_week");
    expect_identical(x.iso_year(), r_int(2024), "iso_year");
    expect_identical(x.days_in_month(), r_int(31), "days_in_month");
    expect_identical(x.is_leap_year(), r_true, "is_leap_year");

    // 2024-03-15 (Fri) weekday = 6 with weeks starting Sunday
    expect_identical(x.wday(), r_int(6), "wday, Sunday start");
    // 2024-03-15 (Fri) weekday = 5 with weeks starting Monday
    expect_identical(x.wday(1), r_int(5), "wday, Monday start");

    // Impossible dates
    // 2024-02-30 -> NA
    expect_identical(r_date(2024, 2, 30), r_date::na(), "impossible date");
    // 2023-02-29 -> NA
    expect_identical(r_date(2023, 2, 29), r_date::na(), "non-leap year Feb 29");

    r_date y(2021, 1, 1);
    // 2021-01-01 (Fri): week 1 of 2021, ISO week 53 of 2020
    expect_identical(y.week(), r_int(1), "week");
    expect_identical(y.year(), r_int(2021), "year");
    expect_identical(y.iso_week(), r_int(53), "iso_week");
    expect_identical(y.iso_year(), r_int(2020), "iso_year");


    r_date z(2019, 12, 30);
    // 2019-12-30 (Mon): week 52 of 2019, ISO week 1 of 2020
    expect_identical(z.week(), r_int(52), "week");
    expect_identical(z.year(), r_int(2019), "year");
    expect_identical(z.iso_week(), r_int(1), "iso_week");
    expect_identical(z.iso_year(), r_int(2020), "iso_year");
}

[[cppally::register]]
void test_date_add(){

    r_date x(ref_day); // 2024-03-15

    // 2024-03-15 + 10 days = 2024-03-25
    expect_identical(x.add<"days">(10), r_date(2024, 3, 25), "add days");
    // 2024-03-15 + 1 week = 2024-03-22
    expect_identical(x.add<"weeks">(1), r_date(2024, 3, 22), "add weeks");
    // 2024-03-15 + 1 month = 2024-04-15
    expect_identical(x.add<"months">(1), r_date(2024, 4, 15), "add months");
    // 2024-03-15 + 1 year = 2025-03-15
    expect_identical(x.add<"years">(1), r_date(2025, 3, 15), "add years");
    // 2024-03-15 - 10 days = 2024-03-05
    expect_identical(x.add<"days">(-10), r_date(2024, 3, 5), "add negative days");
    // 2024-03-15 + 0 months = 2024-03-15
    expect_identical(x.add<"months">(0), x, "add zero months");
    // 2024-03-15 + Inf days = Inf
    expect_identical(x.add<"days">(r_dbl::inf()), r_date(r_dbl::inf()), "add Inf days");
    // 2024-03-15 + Inf months = Inf
    expect_identical(x.add<"months">(r_dbl::inf()), r_date(r_dbl::inf()), "add Inf months");
    // 2024-03-15 - Inf days = -Inf
    expect_identical(x.add<"days">(-r_dbl::inf()), r_date(-r_dbl::inf()), "add -Inf days");
    // 2024-03-15 - Inf months = -Inf
    expect_identical(x.add<"months">(-r_dbl::inf()), r_date(-r_dbl::inf()), "add -Inf months");

    // Sub-day units return fractional r_dates
    // 2024-03-15 + 12 hours = 2024-03-15 12:00 (19797.5)
    expect_identical(x.add<"hours">(12), r_date(19797.5), "add hours");
    // 2024-03-15 + 720 minutes = 2024-03-15 12:00 (19797.5)
    expect_identical(x.add<"minutes">(720), r_date(19797.5), "add minutes");
    // 2024-03-15 + 43200 seconds = 2024-03-15 12:00 (19797.5)
    expect_identical(x.add<"seconds">(43200), r_date(19797.5), "add seconds");

    // Jan 31 + 1 month
    r_date jan31(2024, 1, 31);
    // 2024-01-31 + 1 month = Feb 31 (impossible) -> NA
    expect_identical(jan31.add<"months">(1, roll::none), r_date::na(), "roll none");
    // 2024-01-31 + 1 month = Feb 31 (impossible), backward -> 2024-02-29
    expect_identical(jan31.add<"months">(1, roll::backward), r_date(2024, 2, 29), "roll backward");
    // 2024-01-31 + 1 month = Feb 31 (impossible), forward -> 2024-03-01
    expect_identical(jan31.add<"months">(1, roll::forward), r_date(2024, 3, 1), "roll forward");
    // 2024-01-31 + 1 month = Feb 31 (impossible), away -> 2024-03-01
    expect_identical(jan31.add<"months">(1, roll::away), r_date(2024, 3, 1), "roll away, adding");
    // 2024-01-31 + 1 month = Feb 31 (impossible), nearest -> 2024-02-29
    expect_identical(jan31.add<"months">(1, roll::nearest), r_date(2024, 2, 29), "roll nearest, adding");

    // away rolls forward when adding and backward when subtracting
    r_date mar31(2024, 3, 31);
    // 2024-03-31 - 1 month = Feb 31 (impossible), away -> 2024-02-29
    expect_identical(mar31.add<"months">(-1, roll::away), r_date(2024, 2, 29), "roll away, subtracting");
    // 2024-03-31 - 1 month = Feb 31 (impossible), nearest -> 2024-03-01
    expect_identical(mar31.add<"months">(-1, roll::nearest), r_date(2024, 3, 1), "roll nearest, subtracting");

    // boundary is the start of the next month in either direction and drops a fractional day, forward keeps it
    // 2024-01-31 + 1 month = Feb 31 (impossible), boundary -> 2024-03-01
    expect_identical(jan31.add<"months">(1, roll::boundary), r_date(2024, 3, 1), "roll boundary, adding");
    // 2024-03-31 - 1 month = Feb 31 (impossible), boundary -> 2024-03-01
    expect_identical(mar31.add<"months">(-1, roll::boundary), r_date(2024, 3, 1), "roll boundary, subtracting");
    r_date jan31_noon = jan31.add<"days">(0.5);
    // 2024-01-31 12:00 + 1 month = Feb 31 12:00 (impossible), forward -> 2024-03-01 12:00
    expect_identical(jan31_noon.add<"months">(1, roll::forward), r_date(2024, 3, 1).add<"days">(0.5), "roll forward keeps a fractional day");
    // 2024-01-31 12:00 + 1 month = Feb 31 12:00 (impossible), boundary -> 2024-03-01 00:00
    expect_identical(jan31_noon.add<"months">(1, roll::boundary), r_date(2024, 3, 1), "roll boundary drops a fractional day");

    // 2024-03-31 + 12 months = 2025-03-31
    expect_identical(mar31.add<"months">(12, roll::none), r_date(2025, 3, 31), "month arithmetic with valid date");
}

[[cppally::register]]
void test_date_rounding(){

    r_date x(ref_day); // 2024-03-15

    // 2024-03-15 floored to the year = 2024-01-01
    expect_identical(x.floor<"years">(), r_date(2024, 1, 1), "floor years");
    // 2024-03-15 floored to the month = 2024-03-01
    expect_identical(x.floor<"months">(), r_date(2024, 3, 1), "floor months");
    // 2024-03-15 (Fri) floored to the Sunday-start week = 2024-03-10
    expect_identical(x.floor<"weeks">(), r_date(2024, 3, 10), "floor weeks, Sunday start");
    // 2024-03-15 (Fri) floored to the Monday-start week = 2024-03-11
    expect_identical(x.floor<"weeks">(1), r_date(2024, 3, 11), "floor weeks, Monday start");
    // 2024-03-15 floored to the day = 2024-03-15
    expect_identical(x.floor<"days">(), x, "floor days");

    // 2024-03-15 ceiling to the year = 2025-01-01
    expect_identical(x.ceiling<"years">(), r_date(2025, 1, 1), "ceiling years");
    // 2024-03-15 ceiling to the month = 2024-04-01
    expect_identical(x.ceiling<"months">(), r_date(2024, 4, 1), "ceiling months");
    // 2024-03-15 (Fri) ceiling to the Sunday-start week = 2024-03-17
    expect_identical(x.ceiling<"weeks">(7), r_date(2024, 3, 17), "ceiling weeks, Sunday start");

    // Already on the boundary
    // 2024-03-15 ceiling to the day = 2024-03-15
    expect_identical(x.ceiling<"days">(), x, "ceiling on boundary");
    // 2024-03-01 ceiling to the month = 2024-03-01
    expect_identical(r_date(2024, 3, 1).ceiling<"months">(), r_date(2024, 3, 1), "ceiling on month boundary");

    // 2024-03-15 rounded to the month = 2024-03-01
    expect_identical(x.round<"months">(), r_date(2024, 3, 1), "round months down");
    // 2024-03-20 rounded to the month = 2024-04-01
    expect_identical(r_date(2024, 3, 20).round<"months">(), r_date(2024, 4, 1), "round months up");
    // Ties always floor so noon floors to midnight on the same day
    // 2024-03-15 12:00 rounded to the day = 2024-03-15
    expect_identical(x.add<"hours">(12).round<"days">(), x, "round days with tie");
}

[[cppally::register]]
void test_datetime_accessors(){

    r_psxct x(ref_sec); // 2024-03-15 13:45:30 UTC

    // 2024-03-15 13:45:30 UTC: 1710510330 seconds since epoch, 2024/3/15 13:45:30, day 75 of the year, week 11, ISO week 11 of 2024, 31-day month, leap year
    expect_identical(x.seconds_since_epoch(), r_dbl(1710510330.0), "seconds_since_epoch");
    expect_identical(x.year(), r_int(2024), "year");
    expect_identical(x.month(), r_int(3), "month");
    expect_identical(x.day(), r_int(15), "day");
    expect_identical(x.hour(), r_int(13), "hour");
    expect_identical(x.minute(), r_int(45), "minute");
    expect_identical(x.second(), r_dbl(30.0), "second");
    expect_identical(x.yday(), r_int(75), "yday");
    expect_identical(x.week(), r_int(11), "week");
    expect_identical(x.iso_week(), r_int(11), "iso_week");
    expect_identical(x.iso_year(), r_int(2024), "iso_year");
    expect_identical(x.days_in_month(), r_int(31), "days_in_month");
    expect_identical(x.is_leap_year(), r_true, "is_leap_year");

    // 2024-03-15 (Fri) weekday = 6 with weeks starting Sunday
    expect_identical(x.wday(7), r_int(6), "wday, Sunday start");
    // 2024-03-15 (Fri) weekday = 5 with weeks starting Monday
    expect_identical(x.wday(1), r_int(5), "wday, Monday start");

    // Impossible date-times
    // 2024-02-30 00:00 UTC -> NA
    expect_identical(r_psxct(2024, 2, 30, 0, 0, 0.0), r_psxct::na(), "impossible date");
    // 2024-03-15 24:00 UTC -> NA
    expect_identical(r_psxct(2024, 3, 15, 24, 0, 0.0), r_psxct::na(), "hour out of range");
    // 2024-03-15 00:00:60 UTC -> NA
    expect_identical(r_psxct(2024, 3, 15, 0, 0, 60.0), r_psxct::na(), "second out of range");

    r_psxct y(2021, 1, 1, 12, 0, 0.0);
    // 2021-01-01 12:00 UTC: ISO week 53 of 2020
    expect_identical(y.iso_week(), r_int(53), "iso_week");
    expect_identical(y.iso_year(), r_int(2020), "iso_year");

    // Sub-second precision
    r_psxct frac(2024, 3, 15, 13, 45, 30.25);
    // 2024-03-15 13:45:30.25 UTC: second 30.25, minute 45
    expect_identical(frac.second(), r_dbl(30.25), "fractional second");
    expect_identical(frac.minute(), r_int(45), "minute");
}

[[cppally::register]]
void test_date_conversions(){
    // 2024-03-15 13:45:30 UTC as a date = 2024-03-15
    expect_identical(ref_sec.as_date(), ref_day, "r_psxct.as_date()");
    // 2024-03-15 as a date-time = 2024-03-15 00:00 UTC
    expect_identical(ref_day.as_datetime(), r_psxct(2024, 3, 15, 0, 0, 0.0), "r_date.as_datetime()");
}

[[cppally::register]]
void test_datetime_add(){

    r_psxct x(ref_sec); // 2024-03-15 13:45:30 UTC

    // 2024-03-15 13:45:30 UTC + 30 seconds = 13:46:00
    expect_identical(x.add<"seconds">(30), r_psxct(2024, 3, 15, 13, 46, 0.0), "add seconds");
    // 2024-03-15 13:45:30 UTC + 15 minutes = 14:00:30
    expect_identical(x.add<"minutes">(15), r_psxct(2024, 3, 15, 14, 0, 30.0), "add minutes");
    // 2024-03-15 13:45:30 UTC + 1 hour = 14:45:30
    expect_identical(x.add<"hours">(1), r_psxct(2024, 3, 15, 14, 45, 30.0), "add hours");
    // 2024-03-15 13:45:30 UTC + 1 day = 2024-03-16 13:45:30
    expect_identical(x.add<"days">(1), r_psxct(2024, 3, 16, 13, 45, 30.0), "add days");
    // 2024-03-15 13:45:30 UTC + 1 week = 2024-03-22 13:45:30
    expect_identical(x.add<"weeks">(1), r_psxct(2024, 3, 22, 13, 45, 30.0), "add weeks");
    // 2024-03-15 13:45:30 UTC + 1 month = 2024-04-15 13:45:30
    expect_identical(x.add<"months">(1), r_psxct(2024, 4, 15, 13, 45, 30.0), "add months");
    // 2024-03-15 13:45:30 UTC + 1 year = 2025-03-15 13:45:30
    expect_identical(x.add<"years">(1), r_psxct(2025, 3, 15, 13, 45, 30.0), "add years");
    // 2024-03-15 13:45:30 UTC - 10 days = 2024-03-05 13:45:30
    expect_identical(x.add<"days">(-10), r_psxct(2024, 3, 5, 13, 45, 30.0), "add negative days");
    // 2024-03-15 13:45:30 UTC + 0 months = 2024-03-15 13:45:30
    expect_identical(x.add<"months">(0), x, "add zero months");
    // 2024-03-15 13:45:30 UTC + 0.5 seconds = 13:45:30.5
    expect_identical(x.add<"seconds">(0.5), r_psxct(2024, 3, 15, 13, 45, 30.5), "add fractional seconds");
    // 2024-03-15 13:45:30 UTC + Inf seconds = Inf
    expect_identical(x.add<"seconds">(r_dbl::inf()), r_psxct(r_dbl::inf()), "add Inf seconds");
    // 2024-03-15 13:45:30 UTC + Inf months = Inf
    expect_identical(x.add<"months">(r_dbl::inf()), r_psxct(r_dbl::inf()), "add Inf months");
    // 2024-03-15 13:45:30 UTC - Inf days = -Inf
    expect_identical(x.add<"days">(-r_dbl::inf()), r_psxct(-r_dbl::inf()), "add -Inf days");
    // 2024-03-15 13:45:30 UTC - Inf months = -Inf
    expect_identical(x.add<"months">(-r_dbl::inf()), r_psxct(-r_dbl::inf()), "add -Inf months");

    // Jan 31 + 1 month
    r_psxct jan31(2024, 1, 31, 13, 45, 30.0);
    // 2024-01-31 13:45:30 UTC + 1 month = Feb 31 (impossible) -> NA
    expect_identical(jan31.add<"months">(1, roll::none), r_psxct::na(), "roll none");
    // 2024-01-31 13:45:30 UTC + 1 month = Feb 31 (impossible), backward -> 2024-02-29 13:45:30
    expect_identical(jan31.add<"months">(1, roll::backward), r_psxct(2024, 2, 29, 13, 45, 30.0), "roll backward");
    // 2024-01-31 13:45:30 UTC + 1 month = Feb 31 (impossible), forward -> 2024-03-01 13:45:30
    expect_identical(jan31.add<"months">(1, roll::forward), r_psxct(2024, 3, 1, 13, 45, 30.0), "roll forward");
    // 2024-01-31 13:45:30 UTC + 1 month = Feb 31 (impossible), away -> 2024-03-01 13:45:30
    expect_identical(jan31.add<"months">(1, roll::away), r_psxct(2024, 3, 1, 13, 45, 30.0), "roll away, adding");
    // 2024-01-31 13:45:30 UTC + 1 month = Feb 31 (impossible), nearest -> 2024-02-29 13:45:30
    expect_identical(jan31.add<"months">(1, roll::nearest), r_psxct(2024, 2, 29, 13, 45, 30.0), "roll nearest, adding");

    r_psxct mar31(2024, 3, 31, 13, 45, 30.0);
    // 2024-03-31 13:45:30 UTC - 1 month = Feb 31 (impossible), away -> 2024-02-29 13:45:30
    expect_identical(mar31.add<"months">(-1, roll::away), r_psxct(2024, 2, 29, 13, 45, 30.0), "roll away, subtracting");
    // 2024-03-31 13:45:30 UTC - 1 month = Feb 31 (impossible), nearest -> 2024-03-01 13:45:30
    expect_identical(mar31.add<"months">(-1, roll::nearest), r_psxct(2024, 3, 1, 13, 45, 30.0), "roll nearest, subtracting");

    // 2024-03-31 13:45:30 UTC + 12 months = 2025-03-31 13:45:30
    expect_identical(mar31.add<"months">(12, roll::none), r_psxct(2025, 3, 31, 13, 45, 30.0), "month arithmetic with valid date");
}

[[cppally::register]]
void test_datetime_rounding(){

    r_psxct x(ref_sec); // 2024-03-15 13:45:30 UTC

    // 2024-03-15 13:45:30 UTC floored to the year = 2024-01-01 00:00
    expect_identical(x.floor<"years">(), r_psxct(2024, 1, 1, 0, 0, 0.0), "floor years");
    // 2024-03-15 13:45:30 UTC floored to the month = 2024-03-01 00:00
    expect_identical(x.floor<"months">(), r_psxct(2024, 3, 1, 0, 0, 0.0), "floor months");
    // 2024-03-15 13:45:30 UTC floored to the Sunday-start week = 2024-03-10 00:00
    expect_identical(x.floor<"weeks">(), r_psxct(2024, 3, 10, 0, 0, 0.0), "floor weeks, Sunday start");
    // 2024-03-15 13:45:30 UTC floored to the Monday-start week = 2024-03-11 00:00
    expect_identical(x.floor<"weeks">(1), r_psxct(2024, 3, 11, 0, 0, 0.0), "floor weeks, Monday start");
    // 2024-03-15 13:45:30 UTC floored to the day = 00:00
    expect_identical(x.floor<"days">(), r_psxct(2024, 3, 15, 0, 0, 0.0), "floor days");
    // 2024-03-15 13:45:30 UTC floored to the hour = 13:00
    expect_identical(x.floor<"hours">(), r_psxct(2024, 3, 15, 13, 0, 0.0), "floor hours");
    // 2024-03-15 13:45:30 UTC floored to the minute = 13:45:00
    expect_identical(x.floor<"minutes">(), r_psxct(2024, 3, 15, 13, 45, 0.0), "floor minutes");
    // 2024-03-15 13:45:30 UTC floored to the second = 13:45:30
    expect_identical(x.floor<"seconds">(), x, "floor seconds");

    // 2024-03-15 13:45:30.25 UTC floored to the second = 13:45:30
    expect_identical(r_psxct(2024, 3, 15, 13, 45, 30.25).floor<"seconds">(), x, "floor seconds");

    // 2024-03-15 13:45:30 UTC ceiling to the year = 2025-01-01 00:00
    expect_identical(x.ceiling<"years">(), r_psxct(2025, 1, 1, 0, 0, 0.0), "ceiling years");
    // 2024-03-15 13:45:30 UTC ceiling to the month = 2024-04-01 00:00
    expect_identical(x.ceiling<"months">(), r_psxct(2024, 4, 1, 0, 0, 0.0), "ceiling months");
    // 2024-03-15 13:45:30 UTC ceiling to the Sunday-start week = 2024-03-17 00:00
    expect_identical(x.ceiling<"weeks">(), r_psxct(2024, 3, 17, 0, 0, 0.0), "ceiling weeks");
    // 2024-03-15 13:45:30 UTC ceiling to the day = 2024-03-16 00:00
    expect_identical(x.ceiling<"days">(), r_psxct(2024, 3, 16, 0, 0, 0.0), "ceiling days");
    // 2024-03-15 13:45:30 UTC ceiling to the hour = 14:00
    expect_identical(x.ceiling<"hours">(), r_psxct(2024, 3, 15, 14, 0, 0.0), "ceiling hours");
    // 2024-03-15 13:45:30 UTC ceiling to the minute = 13:46:00
    expect_identical(x.ceiling<"minutes">(), r_psxct(2024, 3, 15, 13, 46, 0.0), "ceiling minutes");

    // 2024-03-15 13:45:30 UTC ceiling to the second = 13:45:30
    expect_identical(x.ceiling<"seconds">(), x, "ceiling on boundary");
    // 2024-03-15 00:00 UTC ceiling to the day = 00:00
    expect_identical(r_psxct(2024, 3, 15, 0, 0, 0.0).ceiling<"days">(), r_psxct(2024, 3, 15, 0, 0, 0.0), "ceiling on day boundary");

    // 2024-03-15 13:45:30 UTC rounded to the hour = 14:00
    expect_identical(x.round<"hours">(), r_psxct(2024, 3, 15, 14, 0, 0.0), "round hours up");
    // 2024-03-15 13:45:30 UTC rounded to the day = 2024-03-16 00:00
    expect_identical(x.round<"days">(), r_psxct(2024, 3, 16, 0, 0, 0.0), "round days up");
    // 2024-03-15 13:20 UTC rounded to the hour = 13:00
    expect_identical(r_psxct(2024, 3, 15, 13, 20, 0.0).round<"hours">(), r_psxct(2024, 3, 15, 13, 0, 0.0), "round hours down");
    // Ties always floor
    // 2024-03-15 13:45:30 UTC rounded to the minute = 13:45:00
    expect_identical(x.round<"minutes">(), r_psxct(2024, 3, 15, 13, 45, 0.0), "round minutes with tie");
}

[[cppally::register]]
void test_date_edge_cases(){

    // Values that are too large (except for Inf) for std::chrono are normalised to NA at construction
    // Inf and NA dates are propagated everywhere where possible and logically correct to do so
    // Date 1e300 days -> NA
    expect_identical(r_date(1e300), r_date::na(), "date out of range");
    // Date-time 1e300 seconds -> NA
    expect_identical(r_psxct(1e300), r_psxct::na(), "datetime out of range");

    // Infinities are preserved
    r_date inf_date(r_dbl::inf());
    r_psxct inf_time(r_dbl::inf());
    // Inf date: days since epoch = Inf
    expect_identical(inf_date.days_since_epoch(), r_dbl::inf(), "date keeps Inf");
    // Inf date-time: seconds since epoch = Inf
    expect_identical(inf_time.seconds_since_epoch(), r_dbl::inf(), "datetime keeps Inf");

    // NA is also propagated
    // NA date: year, weekday and leap year are NA
    expect_identical(r_date::na().year(), r_int::na(), "NA year");
    expect_identical(r_date::na().wday(), r_int::na(), "NA wday");
    expect_identical(r_date::na().is_leap_year(), r_na, "NA is_leap_year");
    // NA date-time: hour, second and date are NA
    expect_identical(r_psxct::na().hour(), r_int::na(), "NA hour");
    expect_identical(r_psxct::na().second(), r_dbl::na(), "NA second");
    expect_identical(r_psxct::na().as_date(), r_date::na(), "NA as_date");

    // Inf has no calendar fields
    // Inf date: year = NA
    expect_identical(inf_date.year(), r_int::na(), "Inf year");
    // Inf date-time: hour = NA
    expect_identical(inf_time.hour(), r_int::na(), "Inf hour");

    // NA and Inf are propagated in arithmetic
    // NA date + 1 day = NA
    expect_identical(r_date::na().add<"days">(1), r_date::na(), "NA add days");
    // NA date + 1 month = NA
    expect_identical(r_date::na().add<"months">(1), r_date::na(), "NA add months");
    // NA date floored to the month = NA
    expect_identical(r_date::na().floor<"months">(), r_date::na(), "NA floor");
    // NA date rounded to the year = NA
    expect_identical(r_date::na().round<"years">(), r_date::na(), "NA round");
    // NA date-time + 1 hour = NA
    expect_identical(r_psxct::na().add<"hours">(1), r_psxct::na(), "NA add hours");
    // NA date-time + 1 month = NA
    expect_identical(r_psxct::na().add<"months">(1), r_psxct::na(), "NA add months");
    // NA date-time ceiling to the day = NA
    expect_identical(r_psxct::na().ceiling<"days">(), r_psxct::na(), "NA ceiling");

    // Inf date + 1 day = Inf
    expect_identical(inf_date.add<"days">(1), inf_date, "Inf add days");
    // Inf date + 1 month = Inf
    expect_identical(inf_date.add<"months">(1), inf_date, "Inf add months");
    // Inf date floored to the month = Inf
    expect_identical(inf_date.floor<"months">(), inf_date, "Inf floor");
    // Inf date-time + 1 hour = Inf
    expect_identical(inf_time.add<"hours">(1), inf_time, "Inf add hours");
    // Inf date-time + 1 month = Inf
    expect_identical(inf_time.add<"months">(1), inf_time, "Inf add months");
    // Inf date-time ceiling to the day = Inf
    expect_identical(inf_time.ceiling<"days">(), inf_time, "Inf ceiling");

    // NA takes precedence over Inf (just like in R)
    // 2024-03-15 + NA days = NA
    expect_identical(r_date(2024, 3, 15).add<"days">(r_dbl::na()), r_date::na(), "NA offset");
    // Inf date + NA months = NA
    expect_identical(inf_date.add<"months">(r_int::na()), r_date::na(), "NA offset on Inf date");
    // Inf date-time + NA months = NA
    expect_identical(inf_time.add<"months">(r_int::na()), r_psxct::na(), "NA offset on Inf date-time");
}

static void expect_near(double x, double target, double tol, const char* what){
    double err = x - target;
    if (!(err <= tol && err >= -tol)){
        abort("test failure for: %s", what);
    }
}

// Round-trip identity should hold: x.add<Unit>(time_diff<Unit>(x, y, width), roll::none, width) == y
template <string_literal Unit, typename T>
static void expect_round_trip(T x, T y, r_dbl n, double tol, const char* what){
    T back = x.template add<Unit>(time_diff<Unit>(x, y, n), roll::none, n);
    expect_near(static_cast<double>(back), static_cast<double>(y), tol, what);
}

[[cppally::register]]
void test_time_diff_units(){

    r_date a(2024, 1, 1);
    r_date b(2024, 1, 2); // day later

    // 2024-01-01 to 2024-01-02 = 1 day
    expect_identical(time_diff<"days">(a, b), r_dbl(1.0), "1 day");
    // 2024-01-01 to 2024-01-02 = 24 hours
    expect_identical(time_diff<"hours">(a, b), r_dbl(24.0), "24 hours");
    // 2024-01-01 to 2024-01-02 = 1440 minutes
    expect_identical(time_diff<"minutes">(a, b), r_dbl(1440.0), "1440 minutes");
    // 2024-01-01 to 2024-01-02 = 86400 seconds
    expect_identical(time_diff<"seconds">(a, b), r_dbl(86400.0), "86400 seconds");
    // 2024-01-01 to 2024-01-02 = 1/7 weeks
    expect_identical(time_diff<"weeks">(a, b), r_dbl(1.0 / 7.0), "1/7 weeks");

    // 2024-01-01 to 2024-01-02 = 12 blocks of 2 hours
    expect_identical(time_diff<"hours">(a, b, r_dbl(2.0)), r_dbl(12.0), "1 day in 2-hour blocks");
    // 2024-01-01 to 2024-01-02 = 0.5 blocks of 2 days
    expect_identical(time_diff<"days">(a, b, r_dbl(2.0)), r_dbl(0.5), "1 day in 2-day blocks");
    // 2024-01-01 to 2024-01-02 = 48 blocks of 30 minutes
    expect_identical(time_diff<"minutes">(a, b, r_dbl(30.0)), r_dbl(48.0), "1 day in 30-minute blocks");

    r_psxct p(2024, 1, 1, 0, 0, 0.0);
    r_psxct q(2024, 1, 2, 0, 0, 0.0);

    // 2024-01-01 00:00 UTC to 2024-01-02 00:00 UTC in hours == the dates in hours
    expect_identical(time_diff<"hours">(p, q), time_diff<"hours">(a, b), "hours");
    // 2024-01-01 00:00 UTC to 2024-01-02 00:00 UTC in minutes == the dates in minutes
    expect_identical(time_diff<"minutes">(p, q), time_diff<"minutes">(a, b), "minutes");
    // 2024-01-01 00:00 UTC to 2024-01-02 00:00 UTC in seconds == the dates in seconds
    expect_identical(time_diff<"seconds">(p, q), time_diff<"seconds">(a, b), "seconds");
    // 2024-01-01 00:00 UTC to 2024-01-02 00:00 UTC in weeks == the dates in weeks
    expect_identical(time_diff<"weeks">(p, q), time_diff<"weeks">(a, b), "weeks");
    // 2024-01-01 00:00 UTC to 2024-01-02 00:00 UTC in blocks of 4 days == the dates in blocks of 4 days
    expect_identical(time_diff<"days">(p, q, r_dbl(4.0)), time_diff<"days">(a, b, r_dbl(4.0)), "4-day blocks");

    // timeline::stopwatch

    r_date c(2025, 3, 15);
    // 2024-01-01 to 2025-03-15 in stopwatch months == the 00:00 UTC date-times in stopwatch months
    expect_identical(time_diff<"months", timeline::stopwatch>(a, c), time_diff<"months", timeline::stopwatch>(a.as_datetime(), c.as_datetime()), "stopwatch months, date vs date-time");
    // 2024-01-01 to 2025-03-15 in stopwatch years == the 00:00 UTC date-times in stopwatch years
    expect_identical(time_diff<"years", timeline::stopwatch>(a, c), time_diff<"years", timeline::stopwatch>(a.as_datetime(), c.as_datetime()), "stopwatch years, date vs date-time");
    // 2024-01-01 to 2025-01-01 = 366 days / 31556952 seconds stopwatch years
    expect_identical(time_diff<"years", timeline::stopwatch>(a, r_date(2025, 1, 1)), r_dbl(366.0 * 86400.0 / 31556952.0), "stopwatch leap year");
    // 2024-01-01 + 1 stopwatch month == + 2629746/86400 days
    expect_identical(a.add<"months", timeline::stopwatch>(1), a.add<"days">(r_dbl(2629746.0 / 86400.0)), "stopwatch month on a date");
    // 2024-01-01 + 1 month = 2024-02-01
    expect_identical(a.add<"months">(1), r_date(2024, 2, 1), "default month on a date");
}

[[cppally::register]]
void test_time_diff_signs(){

    constexpr double fp_ulp = 1e-12;

    r_date a(2019, 1, 1);
    r_date b(2020, 1, 1);

    // 2019-01-01 to 2020-01-01 = 12 months
    expect_identical(time_diff<"months">(a, b), r_dbl(12.0), "whole months, y after x");
    // 2020-01-01 to 2019-01-01 = -12 months
    expect_identical(time_diff<"months">(b, a), r_dbl(-12.0), "whole months, y before x");

    // 2019-01-01 to 2020-01-01 = 2 + 61/152 blocks of 5 months (2 blocks to 2019-11-01, then 61 days of the 152-day block)
    expect_identical(time_diff<"months">(a, b, r_dbl(5.0)), r_dbl(2.0 + 61.0 / 152.0), "months n=5, y after x");
    // 2020-01-01 to 2019-01-01 = -3 + 92/151 blocks of 5 months (-3 blocks to 2018-10-01, then 92 days of the 151-day block)
    expect_identical(time_diff<"months">(b, a, r_dbl(5.0)), r_dbl(-3.0 + 92.0 / 151.0), "months n=5, y before x");

    r_date c(2024, 3, 15);
    r_date d(2026, 11, 20);
    r_psxct p(2024, 3, 15, 13, 45, 30.0);
    r_psxct p2(2024, 3, 15, 14, 45, 30.0);

    // 2024-03-15 to 2026-11-20 in blocks of 0.5 months = 2 x months
    expect_near(time_diff<"months">(c, d, r_dbl(0.5)), 2.0 * time_diff<"months">(c, d), fp_ulp, "width below 1, months");
    // 2024-03-15 to 2026-11-20 in blocks of 0.5 hours = 2 x hours
    expect_identical(time_diff<"hours">(c, d, r_dbl(0.5)), r_dbl(2.0 * static_cast<double>(time_diff<"hours">(c, d))), "width below 1, hours");
    // 2024-03-15 + 1 block of 0.5 months == + 0.5 months
    expect_identical(c.add<"months">(1, roll::none, r_dbl(0.5)), c.add<"months">(r_dbl(0.5)), "add, width below 1");
    // 2024-03-15 13:45:30 UTC + 1 block of 0.5 years == + 6 months
    expect_identical(p.add<"years">(1, roll::none, r_dbl(0.5)), p.add<"months">(6), "add, width below 1, datetime");

    // 2024-03-15 to 2026-11-20 in blocks of -1 month = -(months)
    expect_near(time_diff<"months">(c, d, r_dbl(-1.0)), -static_cast<double>(time_diff<"months">(c, d, r_dbl(1.0))), fp_ulp, "negative width, months");
    // 2026-11-20 to 2024-03-15 in blocks of -5 months = -(blocks of 5 months)
    expect_near(time_diff<"months">(d, c, r_dbl(-5.0)), -static_cast<double>(time_diff<"months">(d, c, r_dbl(5.0))), fp_ulp, "negative width, months, y before x");
    // 2024-03-15 to 2026-11-20 in blocks of -1 year = -(years)
    expect_near(time_diff<"years">(c, d, r_dbl(-1.0)), -static_cast<double>(time_diff<"years">(c, d, r_dbl(1.0))), fp_ulp, "negative width, years");
    // 2024-03-15 to 2026-11-20 in blocks of -3 days = -(blocks of 3 days)
    expect_identical(time_diff<"days">(c, d, r_dbl(-3.0)), r_dbl(-static_cast<double>(time_diff<"days">(c, d, r_dbl(3.0)))), "negative width, days");
    // 2024-03-15 13:45:30 UTC to 14:45:30 = -120 blocks of -30 seconds
    expect_identical(time_diff<"seconds">(p, p2, r_dbl(-30.0)), r_dbl(-120.0), "negative width, datetime");

    // 2024-03-15 + 1 block of -2 days == - 2 days
    expect_identical(c.add<"days">(1, roll::none, r_dbl(-2.0)), c.add<"days">(-2), "add, negative width");
    // 2024-03-15 + 1 block of -3 months == - 3 months
    expect_identical(c.add<"months">(1, roll::none, r_dbl(-3.0)), c.add<"months">(-3), "add, negative width, months");

    // 2019-01-01 to 2019-01-01 in blocks of 0 months = NaN
    expect_identical(time_diff<"months">(a, a, r_dbl(0.0)), r_dbl::nan(), "zero period, same time points");
    // 2019-01-01 to 2020-01-01 in blocks of 0 months = Inf
    expect_identical(time_diff<"months">(a, b, r_dbl(0.0)), r_dbl::inf(), "zero period");
    // 2019-01-01 to 2020-01-01 in blocks of 0 days = Inf
    expect_identical(time_diff<"days">(a, b, r_dbl(0.0)), r_dbl::inf(), "zero period, days");
    // 2020-01-01 to 2019-01-01 in blocks of 0 days = -Inf
    expect_identical(time_diff<"days">(b, a, r_dbl(0.0)), -r_dbl::inf(), "zero period, y before x");
    // 2019-01-01 + 1 block of 0 months = 2019-01-01
    expect_identical(a.add<"months">(1, roll::none, r_dbl(0.0)), a, "zero width, add months");
    // 2019-01-01 + 3 blocks of 0 days = 2019-01-01
    expect_identical(a.add<"days">(3, roll::none, r_dbl(0.0)), a, "zero width, add days");

    // 2019-01-01 to 2020-01-01 in blocks of NA months = NA
    expect_identical(time_diff<"months">(a, b, r_dbl::na()), r_dbl::na(), "NA period");
    // 2019-01-01 to 2020-01-01 in blocks of NA days = NA
    expect_identical(time_diff<"days">(a, b, r_dbl::na()), r_dbl::na(), "NA period, days");
    // 2019-01-01 + 1 block of NA months = NA
    expect_identical(a.add<"months">(1, roll::none, r_dbl::na()), r_date::na(), "NA width, add months");
    // 2019-01-01 + 3 blocks of NA days = NA
    expect_identical(a.add<"days">(3, roll::none, r_dbl::na()), r_date::na(), "NA width, add days");
    // NA to 2020-01-01 in months = NA
    expect_identical(time_diff<"months">(r_date::na(), b), r_dbl::na(), "NA start");
    // 2019-01-01 to NA in months = NA
    expect_identical(time_diff<"months">(a, r_date::na()), r_dbl::na(), "NA end");
}

[[cppally::register]]
void test_time_diff_blocks(){

    constexpr double fp_ulp = 1e-12;

    r_date x(2003, 1, 4);
    r_date y(2003, 1, 1);

    // 2003-01-04 to 2003-01-01 = -3/31 months (block 2002-12-04 to 2003-01-04)
    expect_near(time_diff<"months">(x, y), r_dbl(-3.0 / 31.0), fp_ulp, "3 days of the 31-day month");
    // 2003-01-04 to 2003-01-01 = -3/92 blocks of 3 months (block 2002-10-04 to 2003-01-04)
    expect_near(time_diff<"months">(x, y, r_dbl(3.0)), r_dbl(-3.0 / 92.0), fp_ulp, "3 days of the 92-day quarter");
    // 2003-01-04 to 2003-01-01 = -3/365 blocks of 12 months (block 2002-01-04 to 2003-01-04)
    expect_near(time_diff<"months">(x, y, r_dbl(12.0)), r_dbl(-3.0 / 365.0), fp_ulp, "3 days of the 365-day year");
    // 2003-01-04 to 2003-01-01 = -3/365 years
    expect_near(time_diff<"years">(x, y), r_dbl(-3.0 / 365.0), fp_ulp, "3 days of the 365-day year, years unit");

    r_date lx(2000, 1, 4);
    r_date ly(2000, 1, 1);

    // 2000-01-04 to 2000-01-01 = -3/365 years (block 1999-01-04 to 2000-01-04)
    expect_near(time_diff<"years">(lx, ly), r_dbl(-3.0 / 365.0), fp_ulp, "3 days of the year ending 2000-01-04");
    // 2000-01-01 to 2000-01-04 = 3/366 years (block 2000-01-01 to 2001-01-01)
    expect_near(time_diff<"years">(ly, lx), r_dbl(3.0 / 366.0), fp_ulp, "3 days of the leap year starting 2000-01-01");

    r_date c(2024, 3, 15);
    r_date d(2026, 11, 20);

    // 2024-03-15 to 2026-11-20 in blocks of 1 month == months
    expect_identical(time_diff<"months">(c, d, r_dbl(1.0)), time_diff<"months">(c, d), "k=1 equals plain months");
    // 2026-11-20 to 2024-03-15 in blocks of 1 month == months
    expect_identical(time_diff<"months">(d, c, r_dbl(1.0)), time_diff<"months">(d, c), "k=1 equals plain months, y before x");

    r_date q1(2020, 1, 31);
    r_date q2(2021, 1, 31);

    // 2020-01-31 to 2021-01-31 = 4 blocks of 3 months
    expect_identical(time_diff<"months">(q1, q2, r_dbl(3.0)), r_dbl(4.0), "4 whole quarters");
    // 2021-01-31 to 2020-01-31 = -4 blocks of 3 months
    expect_identical(time_diff<"months">(q2, q1, r_dbl(3.0)), r_dbl(-4.0), "4 whole quarters backwards");
    // 2020-01-31 to 2021-01-31 = 1 year
    expect_identical(time_diff<"years">(q1, q2), r_dbl(1.0), "1 whole year");

    r_date n1(2003, 1, 15);
    r_date n2(2003, 4, 14);

    // 2003-01-15 to 2003-04-14 = -(2 + 30/31) blocks of -1 month
    expect_near(time_diff<"months">(n1, n2, r_dbl(-1.0)), -(2.0 + 30.0 / 31.0), fp_ulp, "negative width, bracket overshoot");

    r_date w1(2003, 1, 15);
    r_date w2(2003, 2, 12);

    // 2003-01-15 to 2003-02-12 = 2 + 3.2/11.8 blocks of 0.4 months
    expect_near(time_diff<"months">(w1, w2, r_dbl(0.4)), 2.0 + 3.2 / 11.8, fp_ulp, "sub-month width, straddling block");

    // 2003-01-31 00:00 UTC to 2003-03-15 00:00 UTC in years == the dates in years
    expect_near(time_diff<"years">(r_psxct(2003, 1, 31, 0, 0, 0.0), r_psxct(2003, 3, 15, 0, 0, 0.0)),
                time_diff<"years">(r_date(2003, 1, 31), r_date(2003, 3, 15)), fp_ulp, "impossible whole month, datetime");
    // 2003-01-31 to 2003-03-15 = 43/365 years
    expect_near(time_diff<"years">(r_date(2003, 1, 31), r_date(2003, 3, 15)), 43.0 / 365.0, fp_ulp, "impossible whole month");

    r_psxct p1(2024, 3, 15, 13, 45, 30.0);
    r_psxct p2(2024, 6, 15, 13, 45, 30.0);

    // 2024-03-15 13:45:30 UTC to 2024-06-15 13:45:30 UTC = 1 block of 3 months
    expect_identical(time_diff<"months">(p1, p2, r_dbl(3.0)), r_dbl(1.0), "1 whole quarter, datetime");
    // 2024-06-15 13:45:30 UTC to 2024-03-15 13:45:30 UTC = -1 block of 3 months
    expect_identical(time_diff<"months">(p2, p1, r_dbl(3.0)), r_dbl(-1.0), "1 whole quarter backwards, datetime");
}

[[cppally::register]]
void test_time_diff_round_trip(){

    r_date a(2019, 1, 1);
    r_date b(2020, 1, 1);
    r_date c(2024, 3, 15);
    r_date d(2026, 11, 20);
    r_date e(2000, 1, 15);
    r_date f(1997, 6, 15);

    constexpr double fp_days = 1e-9;

    // Round trip 2019-01-01 to 2020-01-01 in months
    expect_round_trip<"months">(a, b, r_dbl(1.0), fp_days, "months n=1");
    // Round trip 2020-01-01 to 2019-01-01 in months
    expect_round_trip<"months">(b, a, r_dbl(1.0), fp_days, "months n=1, y before x");
    // Round trip 2024-03-15 to 2026-11-20 in months
    expect_round_trip<"months">(c, d, r_dbl(1.0), fp_days, "months n=1, fractional");
    // Round trip 2000-01-15 to 1997-06-15 in months
    expect_round_trip<"months">(e, f, r_dbl(1.0), fp_days, "months n=1, fractional, y before x");

    // Round trip 2019-01-01 to 2020-01-01 in blocks of 5 months
    expect_round_trip<"months">(a, b, r_dbl(5.0), fp_days, "months n=5");
    // Round trip 2020-01-01 to 2019-01-01 in blocks of 5 months
    expect_round_trip<"months">(b, a, r_dbl(5.0), fp_days, "months n=5, y before x");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 2.5 months
    expect_round_trip<"months">(c, d, r_dbl(2.5), fp_days, "months n=2.5");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 3 months
    expect_round_trip<"months">(c, d, r_dbl(3.0), fp_days, "months n=3");
    // Round trip 2026-11-20 to 2024-03-15 in blocks of 12 months
    expect_round_trip<"months">(d, c, r_dbl(12.0), fp_days, "months n=12, y before x");

    // Round trip 2024-03-15 to 2026-11-20 in years
    expect_round_trip<"years">(c, d, r_dbl(1.0), fp_days, "years n=1");
    // Round trip 2026-11-20 to 2024-03-15 in years
    expect_round_trip<"years">(d, c, r_dbl(1.0), fp_days, "years n=1, y before x");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 2 years
    expect_round_trip<"years">(c, d, r_dbl(2.0), fp_days, "years n=2");
    // Round trip 2000-01-15 to 1997-06-15 in blocks of 2 years
    expect_round_trip<"years">(e, f, r_dbl(2.0), fp_days, "years n=2, y before x");

    // Round trip 2024-03-15 to 2026-11-20 in blocks of -1 month
    expect_round_trip<"months">(c, d, r_dbl(-1.0), fp_days, "months n=-1");
    // Round trip 2000-01-15 to 1997-06-15 in blocks of -1 month
    expect_round_trip<"months">(e, f, r_dbl(-1.0), fp_days, "months n=-1, y before x");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of -12 months
    expect_round_trip<"months">(c, d, r_dbl(-12.0), fp_days, "months n=-12");
    // Round trip 2026-11-20 to 2024-03-15 in blocks of -2 years
    expect_round_trip<"years">(d, c, r_dbl(-2.0), fp_days, "years n=-2, y before x");

    r_date g(2003, 1, 4);
    r_date h(2003, 1, 1);

    // Round trip 2003-01-04 to 2003-01-01 in blocks of 3 months
    expect_round_trip<"months">(g, h, r_dbl(3.0), fp_days, "months n=3, sub-block gap");
    // Round trip 2003-01-04 to 2003-01-01 in blocks of 12 months
    expect_round_trip<"months">(g, h, r_dbl(12.0), fp_days, "months n=12, sub-block gap");
    // Round trip 2003-01-01 to 2003-01-04 in blocks of 12 months
    expect_round_trip<"months">(h, g, r_dbl(12.0), fp_days, "months n=12, sub-block gap forwards");

    r_date i(2003, 1, 15);
    r_date j(2003, 4, 14);

    // Round trip 2003-01-15 to 2003-04-14 in blocks of -1 month
    expect_round_trip<"months">(i, j, r_dbl(-1.0), fp_days, "months n=-1, bracket overshoot");

    r_date w1(2003, 1, 15);
    r_date w2(2003, 2, 12);

    // Round trip 2003-01-15 to 2003-02-12 in blocks of 0.4 months
    expect_round_trip<"months">(w1, w2, r_dbl(0.4), fp_days, "months n=0.4, straddling block");
    // Round trip 2003-02-12 to 2003-01-15 in blocks of 0.4 months
    expect_round_trip<"months">(w2, w1, r_dbl(0.4), fp_days, "months n=0.4, straddling block, y before x");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 0.5 months
    expect_round_trip<"months">(c, d, r_dbl(0.5), fp_days, "months n=0.5");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of -0.4 months
    expect_round_trip<"months">(c, d, r_dbl(-0.4), fp_days, "months n=-0.4");

    // Round trip 2024-03-15 to 2026-11-20 in days
    expect_round_trip<"days">(c, d, r_dbl(1.0), fp_days, "days n=1");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 3 days
    expect_round_trip<"days">(c, d, r_dbl(3.0), fp_days, "days n=3");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of -3 days
    expect_round_trip<"days">(c, d, r_dbl(-3.0), fp_days, "days n=-3");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 2 weeks
    expect_round_trip<"weeks">(c, d, r_dbl(2.0), fp_days, "weeks n=2");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 6 hours
    expect_round_trip<"hours">(c, d, r_dbl(6.0), fp_days, "hours n=6");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 90 minutes
    expect_round_trip<"minutes">(c, d, r_dbl(90.0), fp_days, "minutes n=90");
    // Round trip 2024-03-15 to 2026-11-20 in blocks of 30 seconds
    expect_round_trip<"seconds">(c, d, r_dbl(30.0), fp_days, "seconds n=30");

    r_psxct p(2024, 3, 15, 13, 45, 30.0);
    r_psxct q(2026, 11, 20, 13, 45, 30.0);

    constexpr double fp_secs = 1e-3;

    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in months
    expect_round_trip<"months">(p, q, r_dbl(1.0), fp_secs, "months n=1, datetime");
    // Round trip 2026-11-20 13:45:30 UTC to 2024-03-15 13:45:30 UTC in months
    expect_round_trip<"months">(q, p, r_dbl(1.0), fp_secs, "months n=1, datetime, y before x");
    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in blocks of 5 months
    expect_round_trip<"months">(p, q, r_dbl(5.0), fp_secs, "months n=5, datetime");
    // Round trip 2026-11-20 13:45:30 UTC to 2024-03-15 13:45:30 UTC in blocks of 12 months
    expect_round_trip<"months">(q, p, r_dbl(12.0), fp_secs, "months n=12, datetime, y before x");
    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in years
    expect_round_trip<"years">(p, q, r_dbl(1.0), fp_secs, "years n=1, datetime");
    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in blocks of 2 years
    expect_round_trip<"years">(p, q, r_dbl(2.0), fp_secs, "years n=2, datetime");
    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in blocks of -1 month
    expect_round_trip<"months">(p, q, r_dbl(-1.0), fp_secs, "months n=-1, datetime");
    // Round trip 2026-11-20 13:45:30 UTC to 2024-03-15 13:45:30 UTC in blocks of -12 months
    expect_round_trip<"months">(q, p, r_dbl(-12.0), fp_secs, "months n=-12, datetime, y before x");

    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in seconds
    expect_round_trip<"seconds">(p, q, r_dbl(1.0), fp_secs, "seconds n=1, datetime");
    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in blocks of 6 hours
    expect_round_trip<"hours">(p, q, r_dbl(6.0), fp_secs, "hours n=6, datetime");
    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in blocks of 2 days
    expect_round_trip<"days">(p, q, r_dbl(2.0), fp_secs, "days n=2, datetime");
    // Round trip 2024-03-15 13:45:30 UTC to 2026-11-20 13:45:30 UTC in weeks
    expect_round_trip<"weeks">(p, q, r_dbl(1.0), fp_secs, "weeks n=1, datetime");

    r_psxct r(1959, 6, 22, 18, 30, 0.0);
    r_psxct s(1942, 3, 22, 1, 15, 0.0);
    r_psxct t(1977, 9, 22, 2, 5, 0.0);

    // Round trip 1959-06-22 18:30 UTC to 1977-09-22 02:05 UTC in months
    expect_round_trip<"months">(r, t, r_dbl(1.0), fp_secs, "months n=1, overshoots forwards");
    // Round trip 1977-09-22 02:05 UTC to 1959-06-22 18:30 UTC in months
    expect_round_trip<"months">(t, r, r_dbl(1.0), fp_secs, "months n=1, overshoots backwards");
    // Round trip 1959-06-22 18:30 UTC to 1977-09-22 02:05 UTC in blocks of 5 months
    expect_round_trip<"months">(r, t, r_dbl(5.0), fp_secs, "months n=5, overshoots forwards");
    // Round trip 1959-06-22 18:30 UTC to 1977-09-22 02:05 UTC in blocks of 2 years
    expect_round_trip<"years">(r, t, r_dbl(2.0), fp_secs, "years n=2, overshoots forwards");

    // Round trip 1959-06-22 18:30 UTC to 1942-03-22 01:15 UTC in months
    expect_round_trip<"months">(r, s, r_dbl(1.0), fp_secs, "months n=1, no overshoot backwards");
    // Round trip 1942-03-22 01:15 UTC to 1959-06-22 18:30 UTC in months
    expect_round_trip<"months">(s, r, r_dbl(1.0), fp_secs, "months n=1, no overshoot forwards");
}

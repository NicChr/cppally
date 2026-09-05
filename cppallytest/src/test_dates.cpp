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

    expect_identical(x.wday(), r_int(6), "wday, Sunday start");
    expect_identical(x.wday(1), r_int(5), "wday, Monday start");

    // Impossible dates
    expect_identical(r_date(2024, 2, 30), r_date::na(), "impossible date");
    expect_identical(r_date(2023, 2, 29), r_date::na(), "non-leap year Feb 29");

    r_date y(2021, 1, 1);
    expect_identical(y.week(), r_int(1), "week");
    expect_identical(y.year(), r_int(2021), "year");
    expect_identical(y.iso_week(), r_int(53), "iso_week");
    expect_identical(y.iso_year(), r_int(2020), "iso_year");


    r_date z(2019, 12, 30);
    expect_identical(z.week(), r_int(52), "week");
    expect_identical(z.year(), r_int(2019), "year");
    expect_identical(z.iso_week(), r_int(1), "iso_week");
    expect_identical(z.iso_year(), r_int(2020), "iso_year");
}

[[cppally::register]]
void test_date_add(){

    r_date x(ref_day); // 2024-03-15

    expect_identical(x.add<"days">(10), r_date(2024, 3, 25), "add days");
    expect_identical(x.add<"weeks">(1), r_date(2024, 3, 22), "add weeks");
    expect_identical(x.add<"months">(1), r_date(2024, 4, 15), "add months");
    expect_identical(x.add<"years">(1), r_date(2025, 3, 15), "add years");
    expect_identical(x.add<"days">(-10), r_date(2024, 3, 5), "add negative days");
    expect_identical(x.add<"months">(0), x, "add zero months");
    expect_identical(x.add<"days">(r_dbl::inf()), r_date(r_dbl::inf()), "add Inf days");
    expect_identical(x.add<"months">(r_dbl::inf()), r_date(r_dbl::inf()), "add Inf months");
    expect_identical(x.add<"days">(-r_dbl::inf()), r_date(-r_dbl::inf()), "add -Inf days");
    expect_identical(x.add<"months">(-r_dbl::inf()), r_date(-r_dbl::inf()), "add -Inf months");

    // Sub-day units return fractional r_dates
    expect_identical(x.add<"hours">(12), r_date(19797.5), "add hours");
    expect_identical(x.add<"minutes">(720), r_date(19797.5), "add minutes");
    expect_identical(x.add<"seconds">(43200), r_date(19797.5), "add seconds");

    // Jan 31 + 1 month
    r_date jan31(2024, 1, 31);
    expect_identical(jan31.add<"months">(1, roll::none), r_date::na(), "roll none");
    expect_identical(jan31.add<"months">(1, roll::backward), r_date(2024, 2, 29), "roll backward");
    expect_identical(jan31.add<"months">(1, roll::forward), r_date(2024, 3, 1), "roll forward");
    expect_identical(jan31.add<"months">(1, roll::away), r_date(2024, 3, 1), "roll away, adding");
    expect_identical(jan31.add<"months">(1, roll::nearest), r_date(2024, 2, 29), "roll nearest, adding");

    // away rolls forward when adding and backward when subtracting
    r_date mar31(2024, 3, 31);
    expect_identical(mar31.add<"months">(-1, roll::away), r_date(2024, 2, 29), "roll away, subtracting");
    expect_identical(mar31.add<"months">(-1, roll::nearest), r_date(2024, 3, 1), "roll nearest, subtracting");

    expect_identical(mar31.add<"months">(12, roll::none), r_date(2025, 3, 31), "month arithmetic with valid date");
}

[[cppally::register]]
void test_date_rounding(){

    r_date x(ref_day); // 2024-03-15

    expect_identical(x.floor<"years">(), r_date(2024, 1, 1), "floor years");
    expect_identical(x.floor<"months">(), r_date(2024, 3, 1), "floor months");
    expect_identical(x.floor<"weeks">(), r_date(2024, 3, 10), "floor weeks, Sunday start");
    expect_identical(x.floor<"weeks">(1), r_date(2024, 3, 11), "floor weeks, Monday start");
    expect_identical(x.floor<"days">(), x, "floor days");

    expect_identical(x.ceiling<"years">(), r_date(2025, 1, 1), "ceiling years");
    expect_identical(x.ceiling<"months">(), r_date(2024, 4, 1), "ceiling months");
    expect_identical(x.ceiling<"weeks">(7), r_date(2024, 3, 17), "ceiling weeks, Sunday start");

    // Already on the boundary
    expect_identical(x.ceiling<"days">(), x, "ceiling on boundary");
    expect_identical(r_date(2024, 3, 1).ceiling<"months">(), r_date(2024, 3, 1), "ceiling on month boundary");

    expect_identical(x.round<"months">(), r_date(2024, 3, 1), "round months down");
    expect_identical(r_date(2024, 3, 20).round<"months">(), r_date(2024, 4, 1), "round months up");
    // Ties always floor so noon floors to midnight on the same day
    expect_identical(x.add<"hours">(12).round<"days">(), x, "round days with tie");
}

[[cppally::register]]
void test_datetime_accessors(){

    r_psxct x(ref_sec); // 2024-03-15 13:45:30 UTC

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

    expect_identical(x.wday(7), r_int(6), "wday, Sunday start");
    expect_identical(x.wday(1), r_int(5), "wday, Monday start");

    // Impossible date-times
    expect_identical(r_psxct(2024, 2, 30, 0, 0, 0.0), r_psxct::na(), "impossible date");
    expect_identical(r_psxct(2024, 3, 15, 24, 0, 0.0), r_psxct::na(), "hour out of range");
    expect_identical(r_psxct(2024, 3, 15, 0, 0, 60.0), r_psxct::na(), "second out of range");

    r_psxct y(2021, 1, 1, 12, 0, 0.0);
    expect_identical(y.iso_week(), r_int(53), "iso_week");
    expect_identical(y.iso_year(), r_int(2020), "iso_year");

    // Sub-second precision
    r_psxct frac(2024, 3, 15, 13, 45, 30.25);
    expect_identical(frac.second(), r_dbl(30.25), "fractional second");
    expect_identical(frac.minute(), r_int(45), "minute");
}

[[cppally::register]]
void test_date_conversions(){
    expect_identical(ref_sec.as_date(), ref_day, "r_psxct.as_date()");
    expect_identical(ref_day.as_datetime(), r_psxct(2024, 3, 15, 0, 0, 0.0), "r_date.as_datetime()");
}

[[cppally::register]]
void test_datetime_add(){

    r_psxct x(ref_sec); // 2024-03-15 13:45:30 UTC

    expect_identical(x.add<"seconds">(30), r_psxct(2024, 3, 15, 13, 46, 0.0), "add seconds");
    expect_identical(x.add<"minutes">(15), r_psxct(2024, 3, 15, 14, 0, 30.0), "add minutes");
    expect_identical(x.add<"hours">(1), r_psxct(2024, 3, 15, 14, 45, 30.0), "add hours");
    expect_identical(x.add<"days">(1), r_psxct(2024, 3, 16, 13, 45, 30.0), "add days");
    expect_identical(x.add<"weeks">(1), r_psxct(2024, 3, 22, 13, 45, 30.0), "add weeks");
    expect_identical(x.add<"months">(1), r_psxct(2024, 4, 15, 13, 45, 30.0), "add months");
    expect_identical(x.add<"years">(1), r_psxct(2025, 3, 15, 13, 45, 30.0), "add years");
    expect_identical(x.add<"days">(-10), r_psxct(2024, 3, 5, 13, 45, 30.0), "add negative days");
    expect_identical(x.add<"months">(0), x, "add zero months");
    expect_identical(x.add<"seconds">(0.5), r_psxct(2024, 3, 15, 13, 45, 30.5), "add fractional seconds");
    expect_identical(x.add<"seconds">(r_dbl::inf()), r_psxct(r_dbl::inf()), "add Inf seconds");
    expect_identical(x.add<"months">(r_dbl::inf()), r_psxct(r_dbl::inf()), "add Inf months");
    expect_identical(x.add<"days">(-r_dbl::inf()), r_psxct(-r_dbl::inf()), "add -Inf days");
    expect_identical(x.add<"months">(-r_dbl::inf()), r_psxct(-r_dbl::inf()), "add -Inf months");

    // Jan 31 + 1 month
    r_psxct jan31(2024, 1, 31, 13, 45, 30.0);
    expect_identical(jan31.add<"months">(1, roll::none), r_psxct::na(), "roll none");
    expect_identical(jan31.add<"months">(1, roll::backward), r_psxct(2024, 2, 29, 13, 45, 30.0), "roll backward");
    expect_identical(jan31.add<"months">(1, roll::forward), r_psxct(2024, 3, 1, 13, 45, 30.0), "roll forward");
    expect_identical(jan31.add<"months">(1, roll::away), r_psxct(2024, 3, 1, 13, 45, 30.0), "roll away, adding");
    expect_identical(jan31.add<"months">(1, roll::nearest), r_psxct(2024, 2, 29, 13, 45, 30.0), "roll nearest, adding");

    r_psxct mar31(2024, 3, 31, 13, 45, 30.0);
    expect_identical(mar31.add<"months">(-1, roll::away), r_psxct(2024, 2, 29, 13, 45, 30.0), "roll away, subtracting");
    expect_identical(mar31.add<"months">(-1, roll::nearest), r_psxct(2024, 3, 1, 13, 45, 30.0), "roll nearest, subtracting");

    expect_identical(mar31.add<"months">(12, roll::none), r_psxct(2025, 3, 31, 13, 45, 30.0), "month arithmetic with valid date");
}

[[cppally::register]]
void test_datetime_rounding(){

    r_psxct x(ref_sec); // 2024-03-15 13:45:30 UTC

    expect_identical(x.floor<"years">(), r_psxct(2024, 1, 1, 0, 0, 0.0), "floor years");
    expect_identical(x.floor<"months">(), r_psxct(2024, 3, 1, 0, 0, 0.0), "floor months");
    expect_identical(x.floor<"weeks">(), r_psxct(2024, 3, 10, 0, 0, 0.0), "floor weeks, Sunday start");
    expect_identical(x.floor<"weeks">(1), r_psxct(2024, 3, 11, 0, 0, 0.0), "floor weeks, Monday start");
    expect_identical(x.floor<"days">(), r_psxct(2024, 3, 15, 0, 0, 0.0), "floor days");
    expect_identical(x.floor<"hours">(), r_psxct(2024, 3, 15, 13, 0, 0.0), "floor hours");
    expect_identical(x.floor<"minutes">(), r_psxct(2024, 3, 15, 13, 45, 0.0), "floor minutes");
    expect_identical(x.floor<"seconds">(), x, "floor seconds");

    expect_identical(r_psxct(2024, 3, 15, 13, 45, 30.25).floor<"seconds">(), x, "floor seconds");

    expect_identical(x.ceiling<"years">(), r_psxct(2025, 1, 1, 0, 0, 0.0), "ceiling years");
    expect_identical(x.ceiling<"months">(), r_psxct(2024, 4, 1, 0, 0, 0.0), "ceiling months");
    expect_identical(x.ceiling<"weeks">(), r_psxct(2024, 3, 17, 0, 0, 0.0), "ceiling weeks");
    expect_identical(x.ceiling<"days">(), r_psxct(2024, 3, 16, 0, 0, 0.0), "ceiling days");
    expect_identical(x.ceiling<"hours">(), r_psxct(2024, 3, 15, 14, 0, 0.0), "ceiling hours");
    expect_identical(x.ceiling<"minutes">(), r_psxct(2024, 3, 15, 13, 46, 0.0), "ceiling minutes");

    expect_identical(x.ceiling<"seconds">(), x, "ceiling on boundary");
    expect_identical(r_psxct(2024, 3, 15, 0, 0, 0.0).ceiling<"days">(), r_psxct(2024, 3, 15, 0, 0, 0.0), "ceiling on day boundary");

    expect_identical(x.round<"hours">(), r_psxct(2024, 3, 15, 14, 0, 0.0), "round hours up");
    expect_identical(x.round<"days">(), r_psxct(2024, 3, 16, 0, 0, 0.0), "round days up");
    expect_identical(r_psxct(2024, 3, 15, 13, 20, 0.0).round<"hours">(), r_psxct(2024, 3, 15, 13, 0, 0.0), "round hours down");
    // Ties always floor
    expect_identical(x.round<"minutes">(), r_psxct(2024, 3, 15, 13, 45, 0.0), "round minutes with tie");
}

[[cppally::register]]
void test_date_edge_cases(){

    // Values that are too large (except for Inf) for std::chrono are normalised to NA at construction
    // Inf and NA dates are propagated everywhere where possible and logically correct to do so
    expect_identical(r_date(1e300), r_date::na(), "date out of range");
    expect_identical(r_psxct(1e300), r_psxct::na(), "datetime out of range");

    // Infinities are preserved
    r_date inf_date(r_dbl::inf());
    r_psxct inf_time(r_dbl::inf());
    expect_identical(inf_date.days_since_epoch(), r_dbl::inf(), "date keeps Inf");
    expect_identical(inf_time.seconds_since_epoch(), r_dbl::inf(), "datetime keeps Inf");

    // NA is also propagated
    expect_identical(r_date::na().year(), r_int::na(), "NA year");
    expect_identical(r_date::na().wday(), r_int::na(), "NA wday");
    expect_identical(r_date::na().is_leap_year(), r_na, "NA is_leap_year");
    expect_identical(r_psxct::na().hour(), r_int::na(), "NA hour");
    expect_identical(r_psxct::na().second(), r_dbl::na(), "NA second");
    expect_identical(r_psxct::na().as_date(), r_date::na(), "NA as_date");

    // Inf has no calendar fields
    expect_identical(inf_date.year(), r_int::na(), "Inf year");
    expect_identical(inf_time.hour(), r_int::na(), "Inf hour");

    // NA and Inf are propagated in arithmetic
    expect_identical(r_date::na().add<"days">(1), r_date::na(), "NA add days");
    expect_identical(r_date::na().add<"months">(1), r_date::na(), "NA add months");
    expect_identical(r_date::na().floor<"months">(), r_date::na(), "NA floor");
    expect_identical(r_date::na().round<"years">(), r_date::na(), "NA round");
    expect_identical(r_psxct::na().add<"hours">(1), r_psxct::na(), "NA add hours");
    expect_identical(r_psxct::na().add<"months">(1), r_psxct::na(), "NA add months");
    expect_identical(r_psxct::na().ceiling<"days">(), r_psxct::na(), "NA ceiling");

    expect_identical(inf_date.add<"days">(1), inf_date, "Inf add days");
    expect_identical(inf_date.add<"months">(1), inf_date, "Inf add months");
    expect_identical(inf_date.floor<"months">(), inf_date, "Inf floor");
    expect_identical(inf_time.add<"hours">(1), inf_time, "Inf add hours");
    expect_identical(inf_time.add<"months">(1), inf_time, "Inf add months");
    expect_identical(inf_time.ceiling<"days">(), inf_time, "Inf ceiling");

    // NA takes precedence over Inf (just like in R)
    expect_identical(r_date(2024, 3, 15).add<"days">(r_dbl::na()), r_date::na(), "NA offset");
    expect_identical(inf_date.add<"months">(r_int::na()), r_date::na(), "NA offset on Inf date");
    expect_identical(inf_time.add<"months">(r_int::na()), r_psxct::na(), "NA offset on Inf date-time");
}

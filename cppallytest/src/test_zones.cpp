#include "cppallytest.h"
using namespace cppally;

// Set TZ envvar to America/New_York before testing.
// 2024 transitions: London 01:00 UTC on Mar 31 and Oct 27, New York 07:00 UTC on Mar 10 and 06:00 UTC on Nov 3

[[cppally::register]]
void test_timezones(){

    // Field accessors

    const char* utc_aliases[] = {"Etc/UTC", "Etc/UCT", "Etc/Universal", "Etc/Zulu", "GMT", "Etc/GMT", "GMT0", "Greenwich"};

    // London: LMT (-0:01:15) until 1847, BST since 1916. New York: LMT (-4:56:02) until 1883, DST since 1918
    auto test_with_year = [&](int y){
        r_psxct summer(y, 7, 1, 12, 0, 0.0); // Jul 1 12:00 UTC
        r_psxct new_year(y, 1, 1, 0, 0, 30.0); // Jan 1 00:00:30 UTC
        r_int london_hour(y < 1847 ? 11 : (y < 1916 ? 12 : 13));
        r_int london_month(y < 1847 ? 12 : 1);
        r_int ny_hour(y < 1918 ? 7 : 8);

        // y-07-01 12:00 UTC: hour 13 in London/GB (11 before 1847, 12 before 1916), 8 in New York/session (7 before 1918), 12 in UTC
        expect_identical(summer.with_tzone("Europe/London").hour(), london_hour, "London hour");
        expect_identical(summer.with_tzone("GB").hour(), london_hour, "GB hour");
        expect_identical(summer.with_tzone("America/New_York").hour(), ny_hour, "New York hour");
        expect_identical(summer.with_tzone("").hour(), ny_hour, "session hour");
        expect_identical(summer.with_tzone("UTC").hour(), r_int(12), "UTC hour");

        // y-01-01 00:00:30 UTC: month 1 in London/GB (12 before 1847), 12 in New York/session, 1 in UTC
        expect_identical(new_year.with_tzone("Europe/London").month(), london_month, "London month");
        expect_identical(new_year.with_tzone("GB").month(), london_month, "GB month");
        expect_identical(new_year.with_tzone("America/New_York").month(), r_int(12), "New York month");
        expect_identical(new_year.with_tzone("").month(), r_int(12), "session month");
        expect_identical(new_year.with_tzone("UTC").month(), r_int(1), "UTC month");

        for (const char* alias : utc_aliases){
            // UTC aliases: y-07-01 12:00 UTC hour = 12, y-01-01 00:00:30 UTC month = 1
            expect_identical(summer.with_tzone(alias).hour(), r_int(12), alias);
            expect_identical(new_year.with_tzone(alias).month(), r_int(1), alias);
        }
    };

    // Years 1600 to 2250
    for (int y = 1600; y <= 2250; y += 50){
        test_with_year(y);
    }
    test_with_year(2004); // leap year

    // Abbreviations, including LMT and BST past the 2100 table horizon
    r_psxct summer(2004, 7, 1, 12, 0, 0.0);
    r_psxct winter(2004, 1, 1, 12, 0, 0.0);
    // 2004-07-01 12:00 UTC in GB = 13:00 BST
    expect_identical(summer.with_tzone("GB").datetime_str(), r_str("2004-07-01 13:00:00 BST"), "GB summer abbreviation");
    // 2004-01-01 12:00 UTC in GB = 12:00 GMT
    expect_identical(winter.with_tzone("GB").datetime_str(), r_str("2004-01-01 12:00:00 GMT"), "GB winter abbreviation");
    // 2004-07-01 12:00 UTC in New York = 08:00 EDT
    expect_identical(summer.with_tzone("America/New_York").datetime_str(), r_str("2004-07-01 08:00:00 EDT"), "New York summer abbreviation");
    // 2004-01-01 12:00 UTC in New York = 07:00 EST
    expect_identical(winter.with_tzone("America/New_York").datetime_str(), r_str("2004-01-01 07:00:00 EST"), "New York winter abbreviation");
    // 2004-07-01 12:00 UTC in Etc/UTC = 12:00 UTC
    expect_identical(summer.with_tzone("Etc/UTC").datetime_str(), r_str("2004-07-01 12:00:00 UTC"), "Etc/UTC abbreviation");
    // 2004-07-01 12:00 UTC in GMT = 12:00 GMT
    expect_identical(summer.with_tzone("GMT").datetime_str(), r_str("2004-07-01 12:00:00 GMT"), "GMT abbreviation");
    // 1800-07-01 12:00 UTC in GB = 11:58:45 LMT
    expect_identical(r_psxct(1800, 7, 1, 12, 0, 0.0).with_tzone("GB").datetime_str(), r_str("1800-07-01 11:58:45 LMT"), "London LMT");
    // 1600-07-01 12:00 UTC in New York = 07:03:58 LMT
    expect_identical(r_psxct(1600, 7, 1, 12, 0, 0.0).with_tzone("America/New_York").datetime_str(), r_str("1600-07-01 07:03:58 LMT"), "New York LMT");
    // 2250-07-01 12:00 UTC in GB = 13:00 BST
    expect_identical(r_psxct(2250, 7, 1, 12, 0, 0.0).with_tzone("GB").datetime_str(), r_str("2250-07-01 13:00:00 BST"), "BST in 2250");

    auto add_clock_hour = [](r_psxct cl){ return cl.add<"hours">(1); };

    r_psxct before_gap(2024, 3, 31, 0, 30, 0.0, "Europe/London"); // 00:30 GMT, then 01:00 GMT -> 02:00 BST
    // 2024-03-31 00:30 GMT + 1 stopwatch hour = 02:30 BST
    expect_identical(before_gap.add<"hours">(1), r_psxct(2024, 3, 31, 1, 30, 0.0), "duration hour over the gap");
    // 2024-03-31 00:30 GMT + 1 stopwatch hour = 02:30 BST, hour 2
    expect_identical(before_gap.add<"hours">(1).hour(), r_int(2), "duration hour over the gap is 02:30 BST");
    // 2024-03-31 00:30 GMT + 1 clock hour = 01:30 (gap) -> NA
    expect_identical(before_gap.using_clock(add_clock_hour), r_psxct::na(), "clock hour into the gap");
    // 2024-03-31 00:30 GMT + 1 clock hour = 01:30 (gap), boundary -> 02:00 BST
    expect_identical(before_gap.using_clock(add_clock_hour, roll::boundary), r_psxct(2024, 3, 31, 1, 0, 0.0), "clock hour into the gap, rolled to the transition");
    // 2024-03-31 00:30 GMT + 1 clock hour = 01:30 (gap), forward -> 02:30 BST
    expect_identical(before_gap.using_clock(add_clock_hour, roll::forward), r_psxct(2024, 3, 31, 1, 30, 0.0), "clock hour into the gap, shifted forward to 02:30 BST");
    // 2024-03-31 00:30 GMT + 1 clock hour = 01:30 (gap), backward -> 00:30 GMT
    expect_identical(before_gap.using_clock(add_clock_hour, roll::backward), before_gap, "clock hour into the gap, shifted backward to 00:30 GMT");

    auto sub_clock_hour = [](r_psxct cl){ return cl.add<"hours">(-1); };
    r_psxct after_gap(2024, 3, 31, 2, 45, 0.0, "Europe/London"); // 02:45 BST, - 1 clock hour is 01:45 (gap)
    // 2024-03-31 00:30 GMT + 1 clock hour = 01:30 (gap), away -> 02:30 BST
    expect_identical(before_gap.using_clock(add_clock_hour, roll::away), before_gap.add<"hours">(1), "clock hour into the gap, away when adding");
    // 2024-03-31 00:30 GMT + 1 clock hour = 01:30 (gap), nearest -> 00:30 GMT
    expect_identical(before_gap.using_clock(add_clock_hour, roll::nearest), before_gap, "clock hour into the gap, nearest when adding");
    // 2024-03-31 02:45 BST - 1 clock hour = 01:45 (gap), away -> 00:45 GMT
    expect_identical(after_gap.using_clock(sub_clock_hour, roll::away), after_gap.add<"hours">(-1), "clock hour back into the gap, away when subtracting");
    // 2024-03-31 02:45 BST - 1 clock hour = 01:45 (gap), nearest -> 02:45 BST
    expect_identical(after_gap.using_clock(sub_clock_hour, roll::nearest), after_gap, "clock hour back into the gap, nearest when subtracting");

    r_psxct before_overlap(2024, 10, 27, 0, 30, 0.0, "Europe/London"); // 00:30 BST, then 02:00 BST -> 01:00 GMT
    // 2024-10-27 00:30 BST + 1 stopwatch hour = 01:30 BST
    expect_identical(before_overlap.add<"hours">(1), r_psxct(2024, 10, 27, 0, 30, 0.0), "duration hour, 01:30 BST");
    // 2024-10-27 00:30 BST + 2 stopwatch hours = 01:30 GMT
    expect_identical(before_overlap.add<"hours">(2), r_psxct(2024, 10, 27, 1, 30, 0.0), "duration 2 hours, 01:30 GMT");
    // 2024-10-27 00:30 BST + 2 stopwatch hours = 01:30 GMT, hour 1
    expect_identical(before_overlap.add<"hours">(2).hour(), r_int(1), "duration 2 hours, wall clock hour");
    // 2024-10-27 00:30 BST + 1 clock hour = 01:30 (overlap) -> NA
    expect_identical(before_overlap.using_clock(add_clock_hour), r_psxct::na(), "clock hour into the overlap");
    // 2024-10-27 00:30 BST + 1 clock hour = 01:30 (overlap), backward -> 01:30 BST
    expect_identical(before_overlap.using_clock(add_clock_hour, roll::none, roll::backward), r_psxct(2024, 10, 27, 0, 30, 0.0), "clock hour into the overlap, earlier");
    // 2024-10-27 00:30 BST + 1 clock hour = 01:30 (overlap), forward -> 01:30 GMT
    expect_identical(before_overlap.using_clock(add_clock_hour, roll::none, roll::forward), r_psxct(2024, 10, 27, 1, 30, 0.0), "clock hour into the overlap, later");

    r_psxct ny_noon(2024, 3, 9, 12, 0, 0.0, "America/New_York"); // EST, the day before clocks go forward
    // 2024-03-09 12:00 EST + 1 clock day = 2024-03-10 12:00 EDT
    expect_identical(ny_noon.add<"days">(1), r_psxct(2024, 3, 10, 12, 0, 0.0, "America/New_York"), "period day over the gap");
    // 2024-03-09 12:00 EST + 1 clock day = 2024-03-10 12:00 EDT, hour 12
    expect_identical(ny_noon.add<"days">(1).hour(), r_int(12), "period day keeps the wall clock");
    // 2024-03-09 12:00 EST + 24 stopwatch hours = 2024-03-10 13:00 EDT, hour 13
    expect_identical(ny_noon.add<"hours">(24).hour(), r_int(13), "24 duration hours");

    r_psxct day_before(2024, 10, 26, 1, 30, 0.0, "Europe/London"); // BST
    // 2024-10-26 01:30 BST + 1 clock day = 2024-10-27 01:30 (overlap) -> NA
    expect_identical(day_before.add<"days">(1), r_psxct::na(), "period day into the overlap");
    // 2024-10-26 01:30 BST + 1 clock day = 2024-10-27 01:30 (overlap), away -> 01:30 GMT
    expect_identical(day_before.add<"days">(1, roll::none, roll::away), r_psxct(2024, 10, 27, 1, 30, 0.0), "period day into the overlap, away");
    // 2024-10-26 01:30 BST + 1 clock day = 2024-10-27 01:30 (overlap), nearest -> 01:30 BST
    expect_identical(day_before.add<"days">(1, roll::none, roll::nearest), r_psxct(2024, 10, 27, 0, 30, 0.0), "period day into the overlap, nearest");

    // forward/backward keep the reading's time of day and fraction, boundary is the exact edge instant

    r_psxct jan31(2024, 1, 31, 13, 45, 15.25, "Europe/London"); // GMT
    // 2024-01-31 13:45:15.25 GMT + 1 clock month = Feb 31 (impossible), forward -> 2024-03-01 13:45:15.25 GMT
    expect_identical(jan31.add<"months">(1, roll::forward), r_psxct(2024, 3, 1, 13, 45, 15.25), "month into an impossible date, forward keeps the time of day");
    // 2024-01-31 13:45:15.25 GMT + 1 clock month = Feb 31 (impossible), backward -> 2024-02-29 13:45:15.25 GMT
    expect_identical(jan31.add<"months">(1, roll::backward), r_psxct(2024, 2, 29, 13, 45, 15.25), "month into an impossible date, backward keeps the time of day");
    // 2024-01-31 13:45:15.25 GMT + 1 clock month = Feb 31 (impossible), boundary -> 2024-03-01 00:00 GMT
    expect_identical(jan31.add<"months">(1, roll::boundary), r_psxct(2024, 3, 1, 0, 0, 0.0), "month into an impossible date, boundary is midnight");

    auto add_clock_minutes = [](r_psxct cl){ return cl.add<"minutes">(20); };

    r_psxct gap_frac(2024, 3, 31, 0, 50, 15.25, "Europe/London"); // 00:50:15.25 GMT, + 20 clock minutes is 01:10:15.25 (gap)
    // 2024-03-31 00:50:15.25 GMT + 20 clock minutes = 01:10:15.25 (gap) -> NA
    expect_identical(gap_frac.using_clock(add_clock_minutes), r_psxct::na(), "clock minutes into the gap");
    // 2024-03-31 00:50:15.25 GMT + 20 clock minutes = 01:10:15.25 (gap), forward -> 02:10:15.25 BST
    expect_identical(gap_frac.using_clock(add_clock_minutes, roll::forward), r_psxct(2024, 3, 31, 1, 10, 15.25), "clock minutes into the gap, forward is 02:10:15.25 BST");
    // 2024-03-31 00:50:15.25 GMT + 20 clock minutes (gap), forward == + 20 stopwatch minutes
    expect_identical(gap_frac.using_clock(add_clock_minutes, roll::forward), gap_frac.add<"minutes">(20), "clock minutes into the gap, forward matches duration minutes");
    // 2024-03-31 00:50:15.25 GMT + 20 clock minutes = 01:10:15.25 (gap), backward -> 00:10:15.25 GMT
    expect_identical(gap_frac.using_clock(add_clock_minutes, roll::backward), r_psxct(2024, 3, 31, 0, 10, 15.25), "clock minutes into the gap, backward is 00:10:15.25 GMT");
    // 2024-03-31 00:50:15.25 GMT + 20 clock minutes = 01:10:15.25 (gap), boundary -> 02:00:00 BST
    expect_identical(gap_frac.using_clock(add_clock_minutes, roll::boundary), r_psxct(2024, 3, 31, 1, 0, 0.0), "clock minutes into the gap, boundary drops the fraction");

    r_psxct overlap_frac(2024, 10, 27, 0, 50, 15.25, "Europe/London"); // 00:50:15.25 BST, + 40 clock minutes is 01:30:15.25 (overlap)
    auto add_clock_40_minutes = [](r_psxct cl){ return cl.add<"minutes">(40); };
    // 2024-10-27 00:50:15.25 BST + 40 clock minutes = 01:30:15.25 (overlap), backward -> 01:30:15.25 BST
    expect_identical(overlap_frac.using_clock(add_clock_40_minutes, roll::none, roll::backward), r_psxct(2024, 10, 27, 0, 30, 15.25), "clock minutes into the overlap, earlier keeps the fraction");
    // 2024-10-27 00:50:15.25 BST + 40 clock minutes = 01:30:15.25 (overlap), forward -> 01:30:15.25 GMT
    expect_identical(overlap_frac.using_clock(add_clock_40_minutes, roll::none, roll::forward), r_psxct(2024, 10, 27, 1, 30, 15.25), "clock minutes into the overlap, later keeps the fraction");
    // 2024-10-27 00:50:15.25 BST + 40 clock minutes = 01:30:15.25 (overlap), boundary -> 01:00:00 GMT
    expect_identical(overlap_frac.using_clock(add_clock_40_minutes, roll::none, roll::boundary), r_psxct(2024, 10, 27, 1, 0, 0.0), "clock minutes into the overlap, boundary drops the fraction");

    // Flooring: days on the wall clock, hours by the wall clock's distance to the hour (never ambiguous)

    r_psxct london_fall_noon(2024, 10, 27, 12, 0, 0.0, "Europe/London"); // GMT
    // 2024-10-27 12:00 GMT floored to the day = 00:00 BST
    expect_identical(london_fall_noon.floor<"days">(), r_psxct(2024, 10, 27, 0, 0, 0.0, "Europe/London"), "period day floor to 00:00 BST");

    // Havana's transitions are at midnight: 00:00 doesn't exist on 2024-03-10 and happens twice on 2024-11-03
    r_psxct havana_gap_noon = r_psxct(2024, 3, 10, 16, 0, 0.0).with_tzone("America/Havana"); // 12:00 CDT
    // 2024-03-10 12:00 CDT floored to the day = 00:00 (gap) -> NA
    expect_identical(havana_gap_noon.floor<"days">(), r_psxct::na(), "period day floor into the gap");
    // 2024-03-10 12:00 CDT floored to the day = 00:00 (gap), forward -> 01:00 CDT
    expect_identical(havana_gap_noon.floor<"days">(7, roll::forward), r_psxct(2024, 3, 10, 5, 0, 0.0, "UTC"), "period day floor into the gap, forward is the transition here");

    r_psxct havana_overlap = r_psxct(2024, 11, 3, 5, 30, 0.0).with_tzone("America/Havana"); // 00:30 CST, the second 00:30
    // 2024-11-03 00:30 CST floored to the day = 00:00 (overlap) -> NA
    expect_identical(havana_overlap.floor<"days">(), r_psxct::na(), "period day floor into the overlap");
    // 2024-11-03 00:30 CST floored to the day = 00:00 (overlap), nearest -> 00:00 CST
    expect_identical(havana_overlap.floor<"days">(7, roll::none, roll::nearest), r_psxct(2024, 11, 3, 5, 0, 0.0, "UTC"), "period day floor into the overlap, nearest is 00:00 CST");
    // 2024-11-03 00:30 CST floored to the day = 00:00 (overlap), away -> 00:00 CDT
    expect_identical(havana_overlap.floor<"days">(7, roll::none, roll::away), r_psxct(2024, 11, 3, 4, 0, 0.0, "UTC"), "period day floor into the overlap, away is 00:00 CDT");

    auto floor_clock_hour = [](r_psxct cl){ return cl.floor<"hours">(); };

    r_psxct first_130 = r_psxct(2024, 10, 27, 0, 30, 0.0).with_tzone("Europe/London"); // 01:30 BST
    r_psxct second_130 = r_psxct(2024, 10, 27, 1, 30, 0.0).with_tzone("Europe/London"); // 01:30 GMT
    // 2024-10-27 01:30 BST floored to the hour = 01:00 BST
    expect_identical(first_130.floor<"hours">(), r_psxct(2024, 10, 27, 0, 0, 0.0, "UTC"), "hour floor, 01:00 BST");
    // 2024-10-27 01:30 GMT floored to the hour = 01:00 GMT
    expect_identical(second_130.floor<"hours">(), r_psxct(2024, 10, 27, 1, 0, 0.0, "UTC"), "hour floor, 01:00 GMT");
    // 2024-10-27 01:30 GMT floored to the clock hour = 01:00 (overlap) -> NA
    expect_identical(second_130.using_clock(floor_clock_hour), r_psxct::na(), "clock hour floor into the overlap");
    // 2024-10-27 01:30 GMT floored to the clock hour = 01:00 (overlap), forward -> 01:00 GMT
    expect_identical(second_130.using_clock(floor_clock_hour, roll::none, roll::forward), r_psxct(2024, 10, 27, 1, 0, 0.0, "UTC"), "clock hour floor into the overlap, later");

    r_psxct spring_start(2024, 3, 31, 0, 0, 0.0, "Europe/London"); // GMT
    r_psxct spring_end(2024, 4, 1, 0, 0, 0.0, "Europe/London"); // BST
    // 2024-03-31 00:00 GMT to 2024-04-01 00:00 BST = 23 stopwatch hours
    expect_identical(time_diff<"hours">(spring_start, spring_end), r_dbl(23.0), "duration hours over the gap");
    // 2024-03-31 00:00 UTC to 2024-04-01 00:00 UTC = 24 stopwatch hours
    expect_identical(time_diff<"hours">(spring_start.replace_tzone("UTC"), spring_end.replace_tzone("UTC")), r_dbl(24.0), "UTC hours over the gap");

    r_psxct fall_start(2024, 11, 3, 0, 0, 0.0, "America/New_York"); // EDT
    r_psxct fall_end(2024, 11, 4, 0, 0, 0.0, "America/New_York"); // EST
    // 2024-11-03 00:00 EDT to 2024-11-04 00:00 EST = 25 stopwatch hours
    expect_identical(time_diff<"hours">(fall_start, fall_end), r_dbl(25.0), "duration hours over the overlap");
    // 2024-11-03 00:00 UTC to 2024-11-04 00:00 UTC = 24 stopwatch hours
    expect_identical(time_diff<"hours">(fall_start.replace_tzone("UTC"), fall_end.replace_tzone("UTC")), r_dbl(24.0), "UTC hours over the overlap");

    r_psxct march(2024, 3, 15, 12, 0, 0.0, "Europe/London"); // GMT
    r_psxct april(2024, 4, 15, 12, 0, 0.0, "Europe/London"); // BST
    // 2024-03-15 12:00 GMT to 2024-04-15 12:00 BST = 1 clock month
    expect_identical(time_diff<"months">(march, april), r_dbl(1.0), "period months over the gap");
    // 2024-04-15 12:00 BST to 2024-03-15 12:00 GMT = -1 clock month
    expect_identical(time_diff<"months">(april, march), r_dbl(-1.0), "period months over the gap, backwards");
    // 2024-03-15 12:00 UTC to 2024-04-15 11:00 UTC = 743/744 clock months
    expect_identical(time_diff<"months">(march.with_tzone("UTC"), april.with_tzone("UTC")), r_dbl(743.0 / 744.0), "UTC months over the gap, an hour short");

    r_psxct sep_bst(2024, 9, 27, 1, 30, 0.0, "Europe/London"); // + 1 clock month is 01:30 on Oct 27 (overlap)
    // A block start in the overlap is the occurrence reached first, so the later 01:30 is an hour into a 31 day + 1 hour block
    // 2024-09-27 01:30 BST to 2024-10-27 01:30 GMT = 1 + 3600/2682000 clock months
    expect_identical(time_diff<"months">(sep_bst, r_psxct(2024, 10, 27, 1, 30, 0.0, "Europe/London", roll::none, roll::forward)), r_dbl(1.0) + r_dbl(3600.0) / r_dbl(2682000.0), "period months into the overlap, later");
    // 2024-09-27 01:30 BST to 2024-10-27 01:30 BST = 1 clock month
    expect_identical(time_diff<"months">(sep_bst, r_psxct(2024, 10, 27, 1, 30, 0.0, "Europe/London", roll::none, roll::backward)), r_dbl(1.0), "period months into the overlap, earlier");
    // Block starts in a gap take the transition, so 03:00 BST is an hour in, with the block 30 min short of 365 days
    // 2023-03-31 01:30 BST to 2024-03-31 03:00 BST = 1 + 3600/31534200 clock years
    expect_identical(time_diff<"years">(r_psxct(2023, 3, 31, 1, 30, 0.0, "Europe/London"), r_psxct(2024, 3, 31, 3, 0, 0.0, "Europe/London")), r_dbl(1.0) + r_dbl(3600.0) / r_dbl(31534200.0), "period years into the gap");
    // 01:00 GMT is the second 01:00, after the first 01:01 BST, so a full day plus 59 min of a 25 hour day
    // 2025-10-25 01:01 BST to 2025-10-26 01:00 GMT = 1 + 3540/90000 clock days
    expect_identical(time_diff<"days">(r_psxct(2025, 10, 25, 1, 1, 0.0, "Europe/London"), r_psxct(2025, 10, 26, 1, 0, 0.0, "Europe/London", roll::none, roll::forward)), r_dbl(1.0) + r_dbl(3540.0) / r_dbl(90000.0), "period day into the overlap, later");
    // 2025-10-25 01:01 BST to 2025-10-26 01:00 BST = 86340/86400 clock days
    expect_identical(time_diff<"days">(r_psxct(2025, 10, 25, 1, 1, 0.0, "Europe/London"), r_psxct(2025, 10, 26, 1, 0, 0.0, "Europe/London", roll::none, roll::backward)), r_dbl(86340.0) / r_dbl(86400.0), "period day into the overlap, earlier");
    // Clock minutes 01:00-01:59 start in the first pass, so the second 01:10 is 11 min into the 61 min block from 01:59 BST
    // 2024-10-27 00:30 BST to 01:10 GMT = 89 + 660/3660 clock minutes
    expect_identical(time_diff<"minutes", timeline::clock>(before_overlap, r_psxct(2024, 10, 27, 1, 10, 0.0, "Europe/London", roll::none, roll::forward)), r_dbl(89.0) + r_dbl(660.0) / r_dbl(3660.0), "clock minutes into the overlap, later");
    // x in an overlap: block starts are measured from x, so neighbours of x stay on their side of it
    // 2024-10-27 01:30 BST to 01:29:30 BST = -0.5 clock minutes
    expect_identical(time_diff<"minutes", timeline::clock>(first_130, r_psxct(2024, 10, 27, 0, 29, 30.0).with_tzone("Europe/London")), r_dbl(-0.5), "clock minutes back from the first pass");
    // 2024-10-27 01:30 GMT to 01:30:30 GMT = 0.5 clock minutes
    expect_identical(time_diff<"minutes", timeline::clock>(second_130, r_psxct(2024, 10, 27, 1, 30, 30.0).with_tzone("Europe/London")), r_dbl(0.5), "clock minutes forward from the second pass");

    // timeline

    // 2024-03-31 00:30 GMT + 1 clock hour = 01:30 (gap) -> NA
    expect_identical(before_gap.add<"hours", timeline::clock>(1), r_psxct::na(), "timeline clock hour into the gap");
    // 2024-03-31 00:30 GMT + 1 clock hour (gap), forward == using_clock version
    expect_identical(before_gap.add<"hours", timeline::clock>(1, roll::forward), before_gap.using_clock(add_clock_hour, roll::forward), "timeline clock hour into the gap matches using_clock");
    // 2024-10-27 00:30 BST + 1 clock hour (overlap), backward == using_clock version
    expect_identical(before_overlap.add<"hours", timeline::clock>(1, roll::none, roll::backward), before_overlap.using_clock(add_clock_hour, roll::none, roll::backward), "timeline clock hour into the overlap matches using_clock");

    // 2024-03-09 12:00 EST + 1 stopwatch day == + 24 stopwatch hours
    expect_identical(ny_noon.add<"days", timeline::stopwatch>(1), ny_noon.add<"hours">(24), "stopwatch day over the gap is 24 hours");
    // 2024-03-09 12:00 EST + 1 stopwatch day = 2024-03-10 13:00 EDT, hour 13
    expect_identical(ny_noon.add<"days", timeline::stopwatch>(1).hour(), r_int(13), "stopwatch day over the gap is 13:00 EDT");

    // 2024-03-31 00:00 GMT to 2024-04-01 00:00 BST = 24 clock hours
    expect_identical(time_diff<"hours", timeline::clock>(spring_start, spring_end), r_dbl(24.0), "timeline clock hours over the gap");
    // 2024-11-03 00:00 EDT to 2024-11-04 00:00 EST = 24 clock hours
    expect_identical(time_diff<"hours", timeline::clock>(fall_start, fall_end), r_dbl(24.0), "timeline clock hours over the overlap");
    // 2024-03-31 00:00 GMT to 2024-04-01 00:00 BST = 1 clock day
    expect_identical(time_diff<"days">(spring_start, spring_end), r_dbl(1.0), "clock day over the gap");
    // 2024-03-31 00:00 GMT to 2024-04-01 00:00 BST = 23/24 stopwatch days
    expect_identical(time_diff<"days", timeline::stopwatch>(spring_start, spring_end), r_dbl(23.0 / 24.0), "stopwatch day over the gap");
    // 2024-11-03 00:00 EDT to 2024-11-04 00:00 EST = 25/24 stopwatch days
    expect_identical(time_diff<"days", timeline::stopwatch>(fall_start, fall_end), r_dbl(25.0 / 24.0), "stopwatch day over the overlap");

    r_psxct jan1(2024, 1, 1, 0, 0, 0.0);
    r_psxct stopwatch_month_end(2024, 1, 31, 10, 29, 6.0);
    // 2024-01-01 00:00 UTC + 1 stopwatch month = 2024-01-31 10:29:06 UTC
    expect_identical(jan1.add<"months", timeline::stopwatch>(1), stopwatch_month_end, "stopwatch month is 2629746 seconds");
    // 2024-01-01 00:00 UTC + 1 stopwatch year = 2024-12-31 05:49:12 UTC
    expect_identical(jan1.add<"years", timeline::stopwatch>(1), r_psxct(2024, 12, 31, 5, 49, 12.0), "stopwatch year is 31556952 seconds");
    // 2024-01-01 00:00 UTC + 1 clock year = 2025-01-01 00:00 UTC
    expect_identical(jan1.add<"years">(1), r_psxct(2025, 1, 1, 0, 0, 0.0), "default year is 12 clock months");
    // 2024-01-01 00:00 UTC to 2024-01-31 10:29:06 UTC = 1 stopwatch month
    expect_identical(time_diff<"months", timeline::stopwatch>(jan1, stopwatch_month_end), r_dbl(1.0), "stopwatch month time_diff");
    // 2024-01-01 00:00 UTC to (+ 2 stopwatch years) = 2 stopwatch years
    expect_identical(time_diff<"years", timeline::stopwatch>(jan1, jan1.add<"years", timeline::stopwatch>(2)), r_dbl(2.0), "stopwatch years time_diff");

    // with_tzone()

    r_psxct instant(2024, 7, 1, 12, 0, 0.0);
    r_psxct london = instant.with_tzone("Europe/London");
    // 2024-07-01 12:00 UTC in London == 2024-07-01 12:00 UTC
    expect_identical(london, instant, "with_tzone keeps the instant");
    // 2024-07-01 12:00 UTC in London, zone = "Europe/London"
    expect_identical(r_str(london.tzone()), r_str("Europe/London"), "with_tzone sets the zone");
    // 2024-07-01 12:00 UTC in London = 13:00 BST, hour 13
    expect_identical(london.hour(), r_int(13), "with_tzone London hour");
    // 2024-07-01 13:00 BST in New York = 08:00 EDT, hour 8
    expect_identical(london.with_tzone("America/New_York").hour(), r_int(8), "with_tzone London to New York");
    // 2024-07-01 13:00 BST in UTC, zone = "UTC"
    expect_identical(r_str(london.with_tzone("UTC").tzone()), r_str("UTC"), "with_tzone back to UTC");
    // 2024-07-01 12:00 UTC in GB, zone = "GB"
    expect_identical(r_str(instant.with_tzone("GB").tzone()), r_str("GB"), "link keeps its own name");
    // 2024-07-01 12:00 UTC in session zone, zone = ""
    expect_identical(r_str(instant.with_tzone("").tzone()), r_str(""), "session zone label");
    // 2024-07-01 12:00 UTC in session zone (TZ = New York) = 08:00 EDT
    expect_identical(instant.with_tzone("").datetime_str(), r_str("2024-07-01 08:00:00 EDT"), "session zone resolves to TZ");
    // 2024-07-01 12:00 UTC in Not/A_Zone -> NA
    expect_identical(instant.with_tzone("Not/A_Zone"), r_psxct::na(), "unknown zone is NA");

    // replace_tzone()

    // 2024-07-01 13:00 BST, zone replaced by New York = 13:00 EDT
    expect_identical(london.replace_tzone("America/New_York"), r_psxct(2024, 7, 1, 13, 0, 0.0, "America/New_York"), "replace_tzone keeps the wall clock");
    // 2024-07-01 13:00 BST, zone replaced by New York = 13:00 EDT, hour 13
    expect_identical(london.replace_tzone("America/New_York").hour(), r_int(13), "replace_tzone hour");
    // 2024-07-01 13:00 BST, zone replaced by UTC = 13:00 UTC
    expect_identical(london.replace_tzone("UTC"), r_psxct(2024, 7, 1, 13, 0, 0.0), "replace_tzone to UTC");
    // 2024-07-01 13:00 BST, zone replaced by GB = 13:00 BST
    expect_identical(london.replace_tzone("GB"), london, "replace_tzone to a link of the same zone");
    // 2024-07-01 13:00 BST, zone replaced by Not/A_Zone -> NA
    expect_identical(london.replace_tzone("Not/A_Zone"), r_psxct::na(), "replace_tzone to an unknown zone");

    r_psxct gap_clock(2024, 3, 10, 2, 30, 0.0); // doesn't exist in New York
    // 2024-03-10 02:30 UTC, zone replaced by New York = 02:30 (gap) -> NA
    expect_identical(gap_clock.replace_tzone("America/New_York"), r_psxct::na(), "replace_tzone into the gap");
    // 2024-03-10 02:30 UTC, zone replaced by New York = 02:30 (gap), boundary -> 03:00 EDT
    expect_identical(gap_clock.replace_tzone("America/New_York", roll::boundary), r_psxct(2024, 3, 10, 7, 0, 0.0), "replace_tzone into the gap, rolled to the transition");
    // 2024-03-10 02:30 UTC, zone replaced by New York = 02:30 (gap), forward -> 03:30 EDT
    expect_identical(gap_clock.replace_tzone("America/New_York", roll::forward), r_psxct(2024, 3, 10, 7, 30, 0.0), "replace_tzone into the gap, shifted forward to 03:30 EDT");
    // 2024-03-10 02:30 UTC, zone replaced by New York = 02:30 (gap), backward -> 01:30 EST
    expect_identical(gap_clock.replace_tzone("America/New_York", roll::backward), r_psxct(2024, 3, 10, 6, 30, 0.0), "replace_tzone into the gap, shifted backward to 01:30 EST");

    r_psxct overlap_clock(2024, 11, 3, 1, 30, 0.0); // happens twice in New York
    // 2024-11-03 01:30 UTC, zone replaced by New York = 01:30 (overlap) -> NA
    expect_identical(overlap_clock.replace_tzone("America/New_York"), r_psxct::na(), "replace_tzone into the overlap");
    // 2024-11-03 01:30 UTC, zone replaced by New York = 01:30 (overlap), backward -> 01:30 EDT
    expect_identical(overlap_clock.replace_tzone("America/New_York", roll::none, roll::backward), r_psxct(2024, 11, 3, 5, 30, 0.0), "replace_tzone into the overlap, earlier (EDT)");
    // 2024-11-03 01:30 UTC, zone replaced by New York = 01:30 (overlap), forward -> 01:30 EST
    expect_identical(overlap_clock.replace_tzone("America/New_York", roll::none, roll::forward), r_psxct(2024, 11, 3, 6, 30, 0.0), "replace_tzone into the overlap, later (EST)");
}

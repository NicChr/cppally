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

        expect_identical(summer.with_tzone("Europe/London").hour(), london_hour, "London hour");
        expect_identical(summer.with_tzone("GB").hour(), london_hour, "GB hour");
        expect_identical(summer.with_tzone("America/New_York").hour(), ny_hour, "New York hour");
        expect_identical(summer.with_tzone("").hour(), ny_hour, "session hour");
        expect_identical(summer.with_tzone("UTC").hour(), r_int(12), "UTC hour");

        expect_identical(new_year.with_tzone("Europe/London").month(), london_month, "London month");
        expect_identical(new_year.with_tzone("GB").month(), london_month, "GB month");
        expect_identical(new_year.with_tzone("America/New_York").month(), r_int(12), "New York month");
        expect_identical(new_year.with_tzone("").month(), r_int(12), "session month");
        expect_identical(new_year.with_tzone("UTC").month(), r_int(1), "UTC month");

        for (const char* alias : utc_aliases){
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
    expect_identical(summer.with_tzone("GB").datetime_str(), r_str("2004-07-01 13:00:00 BST"), "GB summer abbreviation");
    expect_identical(winter.with_tzone("GB").datetime_str(), r_str("2004-01-01 12:00:00 GMT"), "GB winter abbreviation");
    expect_identical(summer.with_tzone("America/New_York").datetime_str(), r_str("2004-07-01 08:00:00 EDT"), "New York summer abbreviation");
    expect_identical(winter.with_tzone("America/New_York").datetime_str(), r_str("2004-01-01 07:00:00 EST"), "New York winter abbreviation");
    expect_identical(summer.with_tzone("Etc/UTC").datetime_str(), r_str("2004-07-01 12:00:00 UTC"), "Etc/UTC abbreviation");
    expect_identical(summer.with_tzone("GMT").datetime_str(), r_str("2004-07-01 12:00:00 GMT"), "GMT abbreviation");
    expect_identical(r_psxct(1800, 7, 1, 12, 0, 0.0).with_tzone("GB").datetime_str(), r_str("1800-07-01 11:58:45 LMT"), "London LMT");
    expect_identical(r_psxct(1600, 7, 1, 12, 0, 0.0).with_tzone("America/New_York").datetime_str(), r_str("1600-07-01 07:03:58 LMT"), "New York LMT");
    expect_identical(r_psxct(2250, 7, 1, 12, 0, 0.0).with_tzone("GB").datetime_str(), r_str("2250-07-01 13:00:00 BST"), "BST in 2250");

    auto add_clock_hour = [](r_psxct cl){ return cl.add<"hours">(1); };

    r_psxct before_gap(2024, 3, 31, 0, 30, 0.0, "Europe/London"); // 00:30 GMT, then 01:00 GMT -> 02:00 BST
    expect_identical(before_gap.add<"hours">(1), r_psxct(2024, 3, 31, 1, 30, 0.0), "duration hour over the gap");
    expect_identical(before_gap.add<"hours">(1).hour(), r_int(2), "duration hour over the gap is 02:30 BST");
    expect_identical(before_gap.using_clock(add_clock_hour), r_psxct::na(), "clock hour into the gap");
    expect_identical(before_gap.using_clock(add_clock_hour, roll::forward), r_psxct(2024, 3, 31, 1, 0, 0.0), "clock hour into the gap, rolled to the transition");

    r_psxct before_overlap(2024, 10, 27, 0, 30, 0.0, "Europe/London"); // 00:30 BST, then 02:00 BST -> 01:00 GMT
    expect_identical(before_overlap.add<"hours">(1), r_psxct(2024, 10, 27, 0, 30, 0.0), "duration hour, 01:30 BST");
    expect_identical(before_overlap.add<"hours">(2), r_psxct(2024, 10, 27, 1, 30, 0.0), "duration 2 hours, 01:30 GMT");
    expect_identical(before_overlap.add<"hours">(2).hour(), r_int(1), "duration 2 hours, wall clock hour");
    expect_identical(before_overlap.using_clock(add_clock_hour), r_psxct::na(), "clock hour into the overlap");
    expect_identical(before_overlap.using_clock(add_clock_hour, roll::none, roll::backward), r_psxct(2024, 10, 27, 0, 30, 0.0), "clock hour into the overlap, earlier");
    expect_identical(before_overlap.using_clock(add_clock_hour, roll::none, roll::forward), r_psxct(2024, 10, 27, 1, 30, 0.0), "clock hour into the overlap, later");

    r_psxct ny_noon(2024, 3, 9, 12, 0, 0.0, "America/New_York"); // EST, the day before clocks go forward
    expect_identical(ny_noon.add<"days">(1), r_psxct(2024, 3, 10, 12, 0, 0.0, "America/New_York"), "period day over the gap");
    expect_identical(ny_noon.add<"days">(1).hour(), r_int(12), "period day keeps the wall clock");
    expect_identical(ny_noon.add<"hours">(24).hour(), r_int(13), "24 duration hours");

    r_psxct day_before(2024, 10, 26, 1, 30, 0.0, "Europe/London"); // BST
    expect_identical(day_before.add<"days">(1), r_psxct::na(), "period day into the overlap");
    expect_identical(day_before.add<"days">(1, roll::none, r_dbl(1.0), roll::away), r_psxct(2024, 10, 27, 1, 30, 0.0), "period day into the overlap, away");
    expect_identical(day_before.add<"days">(1, roll::none, r_dbl(1.0), roll::nearest), r_psxct(2024, 10, 27, 0, 30, 0.0), "period day into the overlap, nearest");

    // Flooring: days on the wall clock, hours by the wall clock's distance to the hour (never ambiguous)

    r_psxct london_fall_noon(2024, 10, 27, 12, 0, 0.0, "Europe/London"); // GMT
    expect_identical(london_fall_noon.floor<"days">(), r_psxct(2024, 10, 27, 0, 0, 0.0, "Europe/London"), "period day floor to 00:00 BST");

    // Havana's transitions are at midnight: 00:00 doesn't exist on 2024-03-10 and happens twice on 2024-11-03
    r_psxct havana_gap_noon = r_psxct(2024, 3, 10, 16, 0, 0.0).with_tzone("America/Havana"); // 12:00 CDT
    expect_identical(havana_gap_noon.floor<"days">(), r_psxct::na(), "period day floor into the gap");
    expect_identical(havana_gap_noon.floor<"days">(7, roll::forward), r_psxct(2024, 3, 10, 5, 0, 0.0, "UTC"), "period day floor into the gap, rolled to the transition");

    r_psxct havana_overlap = r_psxct(2024, 11, 3, 5, 30, 0.0).with_tzone("America/Havana"); // 00:30 CST, the second 00:30
    expect_identical(havana_overlap.floor<"days">(), r_psxct::na(), "period day floor into the overlap");
    expect_identical(havana_overlap.floor<"days">(7, roll::none, roll::nearest), r_psxct(2024, 11, 3, 5, 0, 0.0, "UTC"), "period day floor into the overlap, nearest is 00:00 CST");
    expect_identical(havana_overlap.floor<"days">(7, roll::none, roll::away), r_psxct(2024, 11, 3, 4, 0, 0.0, "UTC"), "period day floor into the overlap, away is 00:00 CDT");

    auto floor_clock_hour = [](r_psxct cl){ return cl.floor<"hours">(); };

    r_psxct first_130 = r_psxct(2024, 10, 27, 0, 30, 0.0).with_tzone("Europe/London"); // 01:30 BST
    r_psxct second_130 = r_psxct(2024, 10, 27, 1, 30, 0.0).with_tzone("Europe/London"); // 01:30 GMT
    expect_identical(first_130.floor<"hours">(), r_psxct(2024, 10, 27, 0, 0, 0.0, "UTC"), "hour floor, 01:00 BST");
    expect_identical(second_130.floor<"hours">(), r_psxct(2024, 10, 27, 1, 0, 0.0, "UTC"), "hour floor, 01:00 GMT");
    expect_identical(second_130.using_clock(floor_clock_hour), r_psxct::na(), "clock hour floor into the overlap");
    expect_identical(second_130.using_clock(floor_clock_hour, roll::none, roll::forward), r_psxct(2024, 10, 27, 1, 0, 0.0, "UTC"), "clock hour floor into the overlap, later");

    r_psxct spring_start(2024, 3, 31, 0, 0, 0.0, "Europe/London"); // GMT
    r_psxct spring_end(2024, 4, 1, 0, 0, 0.0, "Europe/London"); // BST
    expect_identical(time_diff<"hours">(spring_start, spring_end), r_dbl(23.0), "duration hours over the gap");
    expect_identical(time_diff<"hours">(spring_start.replace_tzone("UTC"), spring_end.replace_tzone("UTC")), r_dbl(24.0), "clock hours over the gap");

    r_psxct fall_start(2024, 11, 3, 0, 0, 0.0, "America/New_York"); // EDT
    r_psxct fall_end(2024, 11, 4, 0, 0, 0.0, "America/New_York"); // EST
    expect_identical(time_diff<"hours">(fall_start, fall_end), r_dbl(25.0), "duration hours over the overlap");
    expect_identical(time_diff<"hours">(fall_start.replace_tzone("UTC"), fall_end.replace_tzone("UTC")), r_dbl(24.0), "clock hours over the overlap");

    r_psxct march(2024, 3, 15, 12, 0, 0.0, "Europe/London"); // GMT
    r_psxct april(2024, 4, 15, 12, 0, 0.0, "Europe/London"); // BST
    expect_identical(time_diff<"months">(march, april), r_dbl(1.0), "period months over the gap");
    expect_identical(time_diff<"months">(april, march), r_dbl(-1.0), "period months over the gap, backwards");
    expect_identical(time_diff<"months">(march.with_tzone("UTC"), april.with_tzone("UTC")), r_dbl(743.0 / 744.0), "duration months over the gap, an hour short");

    // with_tzone()

    r_psxct instant(2024, 7, 1, 12, 0, 0.0);
    r_psxct london = instant.with_tzone("Europe/London");
    expect_identical(london, instant, "with_tzone keeps the instant");
    expect_identical(r_str(london.tzone()), r_str("Europe/London"), "with_tzone sets the zone");
    expect_identical(london.hour(), r_int(13), "with_tzone London hour");
    expect_identical(london.with_tzone("America/New_York").hour(), r_int(8), "with_tzone London to New York");
    expect_identical(r_str(london.with_tzone("UTC").tzone()), r_str("UTC"), "with_tzone back to UTC");
    expect_identical(r_str(instant.with_tzone("GB").tzone()), r_str("GB"), "link keeps its own name");
    expect_identical(r_str(instant.with_tzone("").tzone()), r_str(""), "session zone label");
    expect_identical(instant.with_tzone("").datetime_str(), r_str("2024-07-01 08:00:00 EDT"), "session zone resolves to TZ");
    expect_identical(instant.with_tzone("Not/A_Zone"), r_psxct::na(), "unknown zone is NA");

    // replace_tzone()

    expect_identical(london.replace_tzone("America/New_York"), r_psxct(2024, 7, 1, 13, 0, 0.0, "America/New_York"), "replace_tzone keeps the wall clock");
    expect_identical(london.replace_tzone("America/New_York").hour(), r_int(13), "replace_tzone hour");
    expect_identical(london.replace_tzone("UTC"), r_psxct(2024, 7, 1, 13, 0, 0.0), "replace_tzone to UTC");
    expect_identical(london.replace_tzone("GB"), london, "replace_tzone to a link of the same zone");
    expect_identical(london.replace_tzone("Not/A_Zone"), r_psxct::na(), "replace_tzone to an unknown zone");

    r_psxct gap_clock(2024, 3, 10, 2, 30, 0.0); // doesn't exist in New York
    expect_identical(gap_clock.replace_tzone("America/New_York"), r_psxct::na(), "replace_tzone into the gap");
    expect_identical(gap_clock.replace_tzone("America/New_York", roll::forward), r_psxct(2024, 3, 10, 7, 0, 0.0), "replace_tzone into the gap, rolled to the transition");

    r_psxct overlap_clock(2024, 11, 3, 1, 30, 0.0); // happens twice in New York
    expect_identical(overlap_clock.replace_tzone("America/New_York"), r_psxct::na(), "replace_tzone into the overlap");
    expect_identical(overlap_clock.replace_tzone("America/New_York", roll::none, roll::backward), r_psxct(2024, 11, 3, 5, 30, 0.0), "replace_tzone into the overlap, earlier (EDT)");
    expect_identical(overlap_clock.replace_tzone("America/New_York", roll::none, roll::forward), r_psxct(2024, 11, 3, 6, 30, 0.0), "replace_tzone into the overlap, later (EST)");
}

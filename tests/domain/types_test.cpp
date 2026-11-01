// localDay / Clock / epoch seconds. Times are built with std::mktime from local fields,
// so the tests do not depend on the machine's time zone (the ATM itself needs Kyiv).
#include "support/mini_test.hpp"

#include "support/domain_assertions.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "domain/types.hpp"

namespace {

    using atm::domain::Clock;
    using atm::domain::ErrorReason;
    using atm::domain::fromEpochSeconds;
    using atm::domain::localDay;
    using atm::domain::TimePoint;
    using atm::domain::toEpochSeconds;
    using std::chrono::milliseconds;

    // Local moment; tm_isdst = -1 lets the system decide about daylight saving time.
    // Most dates used are far from DST transitions; the DST-day tests avoid the 03:00-04:00 hour.
    TimePoint makeLocal(int year, int month, int day, int hour, int minute, int second) {
        std::tm fields{};
        fields.tm_year = year - 1900;
        fields.tm_mon = month - 1;
        fields.tm_mday = day;
        fields.tm_hour = hour;
        fields.tm_min = minute;
        fields.tm_sec = second;
        fields.tm_isdst = -1;
        const std::time_t t = std::mktime(&fields);
        if (t == static_cast<std::time_t>(-1)) {
            throw std::runtime_error("mktime failed in the test helper");
        }
        return std::chrono::system_clock::from_time_t(t);
    }

    static_assert(noexcept(atm::domain::toEpochSeconds(std::declval<TimePoint>())));

    // Representable whole-second bounds used for boundary checks.
    constexpr std::int64_t kMinSeconds =
        std::chrono::ceil<std::chrono::seconds>((TimePoint::duration::min)()).count();
    constexpr std::int64_t kMaxSeconds =
        std::chrono::floor<std::chrono::seconds>((TimePoint::duration::max)()).count();

    // --- localDay -----------------------------------------------------------------

    TEST(TypesTest, LocalDayFormatsAsIsoDate) {
        EXPECT_EQ(localDay(makeLocal(2026, 6, 15, 12, 0, 0)), "2026-06-15");
    }

    TEST(TypesTest, LocalDayHasExactlyTenCharactersAndPadsWithZeros) {
        const std::string day = localDay(makeLocal(2026, 1, 5, 9, 30, 0));
        EXPECT_EQ(day, "2026-01-05");
        EXPECT_EQ(day.size(), std::size_t{ 10 });
    }

    TEST(TypesTest, LocalDayHandlesLeapDay) {
        EXPECT_EQ(localDay(makeLocal(2024, 2, 29, 12, 0, 0)), "2024-02-29");
        EXPECT_EQ(localDay(makeLocal(2024, 3, 1, 0, 0, 0)), "2024-03-01");
    }

    TEST(TypesTest, LocalDayJustBeforeMidnightIsStillPreviousDay) {
        EXPECT_EQ(localDay(makeLocal(2026, 6, 15, 23, 59, 59)), "2026-06-15");
    }

    TEST(TypesTest, LocalDayAtMidnightIsNextDay) {
        EXPECT_EQ(localDay(makeLocal(2026, 6, 16, 0, 0, 0)), "2026-06-16");
    }

    TEST(TypesTest, LocalDayCrossesMonthAndYearBoundary) {
        EXPECT_EQ(localDay(makeLocal(2026, 12, 31, 23, 59, 59)), "2026-12-31");
        EXPECT_EQ(localDay(makeLocal(2027, 1, 1, 0, 0, 0)), "2027-01-01");
        EXPECT_EQ(localDay(makeLocal(2026, 6, 30, 23, 59, 59)), "2026-06-30");
        EXPECT_EQ(localDay(makeLocal(2026, 7, 1, 0, 0, 0)), "2026-07-01");
    }

    TEST(TypesTest, LocalDayOnDaylightSavingChangeDaysKeepsTheDate) {
        // Kyiv changes the clocks at 03:00/04:00, never at midnight, so every moment keeps its date.
        // 2026-03-29: clocks go forward. 2026-10-25: clocks go back.
        EXPECT_EQ(localDay(makeLocal(2026, 3, 29, 0, 0, 0)), "2026-03-29");
        EXPECT_EQ(localDay(makeLocal(2026, 3, 29, 2, 59, 59)), "2026-03-29");
        EXPECT_EQ(localDay(makeLocal(2026, 3, 29, 12, 0, 0)), "2026-03-29");
        EXPECT_EQ(localDay(makeLocal(2026, 3, 29, 23, 59, 59)), "2026-03-29");
        EXPECT_EQ(localDay(makeLocal(2026, 3, 30, 0, 0, 0)), "2026-03-30");
        EXPECT_EQ(localDay(makeLocal(2026, 10, 25, 0, 0, 0)), "2026-10-25");
        EXPECT_EQ(localDay(makeLocal(2026, 10, 25, 12, 0, 0)), "2026-10-25");
        EXPECT_EQ(localDay(makeLocal(2026, 10, 25, 23, 59, 59)), "2026-10-25");
        EXPECT_EQ(localDay(makeLocal(2026, 10, 26, 0, 0, 0)), "2026-10-26");
    }

    TEST(TypesTest, LocalDayEqualsLocalDayOfTheFlooredSecond) {
        // The original timestamp and its whole-second representation
        // must produce the same local day.
        // WithdrawalService's use of the timestamp is tested separately.
        const TimePoint midnight = makeLocal(2026, 6, 16, 0, 0, 0);
        for (int ms = -1'500; ms <= 1'500; ms += 125) {
            const TimePoint tp = midnight + milliseconds(ms);
            EXPECT_EQ(localDay(tp), localDay(fromEpochSeconds(toEpochSeconds(tp))));
        }
    }

    TEST(TypesTest, LocalDayOfExtremeTimePointsEitherWorksOrThrowsInvalidData) {
        // The result depends on the platform's time_t range (e.g. Windows rejects negative values),
        // but it must never be undefined behaviour or an exception of another type.
        const TimePoint extremes[] = { (TimePoint::max)(), (TimePoint::min)() };
        for (const TimePoint tp : extremes) {
            try {
                EXPECT_FALSE(localDay(tp).empty());
            }
            catch (const atm::domain::DomainError& e) {
                EXPECT_EQ(e.reason(), ErrorReason::InvalidData);
            }
        }
    }

    TEST(TypesTest, LocalDayFloorsFractionsOfSecondLikeTheJournalTimestamp) {
        // 23:59:59.900 is still the same day; 00:00:00.100 is the next day.
        EXPECT_EQ(localDay(makeLocal(2026, 6, 15, 23, 59, 59) + milliseconds(900)), "2026-06-15");
        EXPECT_EQ(localDay(makeLocal(2026, 6, 16, 0, 0, 0) + milliseconds(100)), "2026-06-16");
        // 100 ms before midnight floors to 23:59:59 of the previous day, as at_utc does.
        EXPECT_EQ(localDay(makeLocal(2026, 6, 16, 0, 0, 0) - milliseconds(100)), "2026-06-15");
    }

    // --- epoch seconds -----------------------------------------------------------------

    TEST(TypesTest, EpochSecondsRoundTripForWholeSeconds) {
        const TimePoint tp = makeLocal(2026, 6, 15, 12, 0, 0);
        EXPECT_EQ(fromEpochSeconds(toEpochSeconds(tp)), tp);
    }

    TEST(TypesTest, EpochSecondsOfEpochIsZero) {
        EXPECT_EQ(toEpochSeconds(TimePoint{}), 0);
        EXPECT_EQ(toEpochSeconds(fromEpochSeconds(1'000'000)), 1'000'000);
    }

    TEST(TypesTest, EpochSecondsKnownValue) {
        // 1'768'471'200 = 2026-01-15T10:00:00Z
        constexpr std::int64_t expectedSeconds = 1'768'471'200;

        const TimePoint expectedTime{
            std::chrono::duration_cast<TimePoint::duration>(
                std::chrono::seconds{expectedSeconds}
            )
        };

        EXPECT_EQ(toEpochSeconds(expectedTime), expectedSeconds);
        EXPECT_EQ(fromEpochSeconds(expectedSeconds), expectedTime);
    }

    TEST(TypesTest, EpochSecondsAreRoundedDown) {
        EXPECT_EQ(toEpochSeconds(TimePoint{} + milliseconds(1'999)), 1);
        EXPECT_EQ(toEpochSeconds(TimePoint{} + milliseconds(999)), 0);
        EXPECT_EQ(toEpochSeconds(TimePoint{} - milliseconds(1)), -1);
        EXPECT_EQ(toEpochSeconds(TimePoint{} - milliseconds(1'000)), -1);
        EXPECT_EQ(toEpochSeconds(TimePoint{} - milliseconds(1'999)), -2);
    }

    TEST(TypesTest, FractionOfSecondIsNotRestoredByFromEpochSeconds) {
        const TimePoint whole = makeLocal(2026, 6, 15, 12, 0, 0);
        EXPECT_EQ(fromEpochSeconds(toEpochSeconds(whole + milliseconds(750))), whole);
    }

    TEST(TypesTest, FromEpochSecondsAcceptsExactBounds) {
        EXPECT_EQ(toEpochSeconds(fromEpochSeconds(kMaxSeconds)), kMaxSeconds);
        EXPECT_EQ(toEpochSeconds(fromEpochSeconds(kMinSeconds)), kMinSeconds);
    }

    TEST(TypesTest, FromEpochSecondsRejectsValuesJustOutsideBounds) {
        EXPECT_DOMAIN_ERROR((void)fromEpochSeconds(kMaxSeconds + 1), ErrorReason::InvalidData);
        EXPECT_DOMAIN_ERROR((void)fromEpochSeconds(kMinSeconds - 1), ErrorReason::InvalidData);
    }

    TEST(TypesTest, FromEpochSecondsRejectsExtremeValues) {
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
        EXPECT_DOMAIN_ERROR((void)fromEpochSeconds(kMax), ErrorReason::InvalidData);
        EXPECT_DOMAIN_ERROR((void)fromEpochSeconds(kMin), ErrorReason::InvalidData);
    }

    // --- Clock ---------------------------------------------------------------------

    TEST(TypesTest, ClockCanBeFixedForTests) {
        const TimePoint fixed = makeLocal(2026, 6, 15, 12, 0, 0);
        const Clock clock = [fixed] { return fixed; };
        EXPECT_EQ(clock(), fixed);
        EXPECT_EQ(clock(), clock());
    }

    TEST(TypesTest, EmptyClockThrowsBadFunctionCall) {
        // Documented in types.hpp: services must check their Clock in the constructor.
        const Clock empty;
        EXPECT_FALSE(static_cast<bool>(empty));
        EXPECT_THROW((void)empty(), std::bad_function_call);
    }

}  // namespace
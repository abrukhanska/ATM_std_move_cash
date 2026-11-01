// Checks the runtime environment separately from domain logic.
// The ATM counts withdrawals per Kyiv calendar day.
// Our CMake configuration sets TZ=Europe/Kyiv for this CTest target on POSIX.
// On Windows, configure the runtime/system time zone for Kyiv,
// including automatic daylight saving adjustment.
#include "support/mini_test.hpp"

#include <chrono>
#include <ctime>
#include <stdexcept>

#include "domain/types.hpp"

namespace {

    atm::domain::TimePoint makeEnvironmentLocal(
        int year,
        int month,
        int day,
        int hour
    ) {
        std::tm fields{};
        fields.tm_year = year - 1900;
        fields.tm_mon = month - 1;
        fields.tm_mday = day;
        fields.tm_hour = hour;
        fields.tm_isdst = -1;

        const std::time_t t = std::mktime(&fields);

        if (t == static_cast<std::time_t>(-1)) {
            throw std::runtime_error(
                "mktime failed in the environment test"
            );
        }

        return std::chrono::system_clock::from_time_t(t);
    }

    TEST(EnvironmentTest, KyivTimeZoneIsConfigured) {
        // 2026-01-15 12:00 Kyiv = 10:00 UTC.
        EXPECT_EQ(
            atm::domain::toEpochSeconds(
                makeEnvironmentLocal(2026, 1, 15, 12)
            ),
            1'768'471'200
        );

        // 2026-07-15 12:00 Kyiv = 09:00 UTC.
        EXPECT_EQ(
            atm::domain::toEpochSeconds(
                makeEnvironmentLocal(2026, 7, 15, 12)
            ),
            1'784'106'000
        );
    }

    TEST(EnvironmentTest, KyivDaylightSavingDaysAreTwentyThreeAndTwentyFiveHoursLong) {
        using atm::domain::toEpochSeconds;
        // 2026-03-29: clocks go forward at 03:00, the calendar day is 23 hours long.
        EXPECT_EQ(toEpochSeconds(makeEnvironmentLocal(2026, 3, 30, 0)) -
            toEpochSeconds(makeEnvironmentLocal(2026, 3, 29, 0)),
            23 * 3600);
        // 2026-10-25: clocks go back at 04:00, the calendar day is 25 hours long.
        EXPECT_EQ(toEpochSeconds(makeEnvironmentLocal(2026, 10, 26, 0)) -
            toEpochSeconds(makeEnvironmentLocal(2026, 10, 25, 0)),
            25 * 3600);
        // An ordinary day.
        EXPECT_EQ(toEpochSeconds(makeEnvironmentLocal(2026, 6, 16, 0)) -
            toEpochSeconds(makeEnvironmentLocal(2026, 6, 15, 0)),
            24 * 3600);
    }

}  // namespace
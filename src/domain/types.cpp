#include "domain/types.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <time.h> 

#include "domain/domain_error.hpp"

namespace atm::domain {

    std::string localDay(TimePoint tp) {
        // Use the same whole-second value as toEpochSeconds (and therefore as the
        // journal's at_utc), so the day and the timestamp can never disagree.
        // Avoid implementation-defined rounding of fractional seconds by to_time_t.
        const std::int64_t seconds = toEpochSeconds(tp);
        const auto t = static_cast<std::time_t>(seconds);
        if (static_cast<std::int64_t>(t) != seconds) {  // time_t narrower than int64_t
            throw DomainError(ErrorReason::InvalidData);
        }

        std::tm local{};
#ifdef _WIN32
        if (localtime_s(&local, &t) != 0) {
            throw DomainError(ErrorReason::InvalidData);
        }
#else
        if (localtime_r(&t, &local) == nullptr) {
            throw DomainError(ErrorReason::InvalidData);
        }
#endif

        std::array<char, 32> buffer{};
        if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d", &local) == 0) {
            throw DomainError(ErrorReason::InvalidData);
        }
        return std::string(buffer.data());
    }

    std::int64_t toEpochSeconds(TimePoint tp) noexcept {
        return std::chrono::floor<std::chrono::seconds>(tp.time_since_epoch()).count();
    }

    TimePoint fromEpochSeconds(std::int64_t seconds) {
        // Smallest and largest whole-second values representable by TimePoint.
        // The lower bound rounds up and the upper bound rounds down, so both
        // stay inside the representable range.
        constexpr std::int64_t minSeconds =
            std::chrono::ceil<std::chrono::seconds>((TimePoint::duration::min)()).count();
        constexpr std::int64_t maxSeconds =
            std::chrono::floor<std::chrono::seconds>((TimePoint::duration::max)()).count();

        if (seconds < minSeconds || seconds > maxSeconds) {
            throw DomainError(ErrorReason::InvalidData);
        }
        return TimePoint{
            std::chrono::duration_cast<TimePoint::duration>(std::chrono::seconds{seconds}) };
    }

}  // namespace atm::domain
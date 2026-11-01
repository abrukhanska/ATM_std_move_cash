#ifndef ATM_DOMAIN_TYPES_HPP
#define ATM_DOMAIN_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

namespace atm::domain {

	using AccountId = std::int64_t;
	using JournalId = std::int64_t;
	using TimePoint = std::chrono::system_clock::time_point;

	// An empty Clock throws std::bad_function_call when called, so services
	// check it in their constructors.
	using Clock = std::function<TimePoint()>;

	// Calendar day in the system local time zone, formatted as "YYYY-MM-DD".
	// Fractional seconds are floored first (same rule as toEpochSeconds), so the
	// day always matches the journal timestamp.
	// The ATM environment must be configured for the Kyiv time zone.
	// Throws DomainError(InvalidData) if the time cannot be converted.
	[[nodiscard]] std::string localDay(TimePoint tp);

	// Whole seconds since 1970-01-01 UTC, rounded down
	// (for JournalEntry::atUtc and the journal.at_utc column).
	[[nodiscard]] std::int64_t toEpochSeconds(TimePoint tp) noexcept;

	// Converts whole Unix seconds to TimePoint.
	// Fractional seconds discarded by toEpochSeconds are not restored.
	// A value outside the TimePoint range causes DomainError(InvalidData).
	[[nodiscard]] TimePoint fromEpochSeconds(std::int64_t seconds);

}  // namespace atm::domain

#endif  // ATM_DOMAIN_TYPES_HPP
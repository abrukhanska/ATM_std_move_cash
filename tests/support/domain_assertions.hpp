#ifndef ATM_TESTS_SUPPORT_DOMAIN_ASSERTIONS_HPP
#define ATM_TESTS_SUPPORT_DOMAIN_ASSERTIONS_HPP

// Domain-specific test helpers shared by all test files.

#include <cstddef>
#include <ostream>

#include "support/mini_test.hpp"

#include "domain/banknote_set.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

// Printing of BanknoteSet so that failures show counts instead of "<value>".
namespace atm::domain {

    inline std::ostream& operator<<(std::ostream& out, const BanknoteSet& set) {
        out << '{';
        for (std::size_t i = 0; i < kDenominationCount; ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << denominationUah(kAllDenominations[i]) << "x" << set.count(kAllDenominations[i]);
        }
        return out << '}';
    }

}  // namespace atm::domain

namespace atm_test {

    // All values of the enums from specification section 3.2.
    inline constexpr atm::domain::ErrorReason kAllErrorReasons[] = {
        atm::domain::ErrorReason::InvalidAmount,
        atm::domain::ErrorReason::Overflow,
        atm::domain::ErrorReason::InvalidRequisite,
        atm::domain::ErrorReason::UnknownCard,
        atm::domain::ErrorReason::UnknownAccount,
        atm::domain::ErrorReason::UnknownPayee,
        atm::domain::ErrorReason::SelfTransfer,
        atm::domain::ErrorReason::IncorrectPin,
        atm::domain::ErrorReason::SamePin,
        atm::domain::ErrorReason::InsufficientFunds,
        atm::domain::ErrorReason::DailyLimitExceeded,
        atm::domain::ErrorReason::AtmOutOfCash,
        atm::domain::ErrorReason::AccessDenied,
        atm::domain::ErrorReason::InvalidPreview,
        atm::domain::ErrorReason::InvalidData };

    inline constexpr atm::domain::EntryKind kAllEntryKinds[] = {
        atm::domain::EntryKind::Withdrawal,      atm::domain::EntryKind::Deposit,
        atm::domain::EntryKind::Transfer,        atm::domain::EntryKind::Payment,
        atm::domain::EntryKind::PinChange,       atm::domain::EntryKind::CardRetained,
        atm::domain::EntryKind::CardReleased,    atm::domain::EntryKind::CashLoaded,
        atm::domain::EntryKind::CashUnloaded,    atm::domain::EntryKind::DeviceInService,
        atm::domain::EntryKind::DeviceOutOfService };

}  // namespace atm_test

// Expects that `stmt` throws DomainError with exactly `expectedReason`.
// Reports the line of the test (not of a helper) and names both reasons.
#define EXPECT_DOMAIN_ERROR(stmt, expectedReason)                                          \
    do {                                                                                   \
        bool caught_ = false;                                                              \
        try {                                                                              \
            stmt;                                                                          \
        } catch (const ::atm::domain::DomainError& error_) {                               \
            caught_ = true;                                                                \
            if (error_.reason() != (expectedReason)) {                                     \
                ::mini_test::Failure(__FILE__, __LINE__)                                   \
                    << "expected reason " << ::atm::domain::toString(expectedReason)       \
                    << " but got " << ::atm::domain::toString(error_.reason()) << " in "   \
                    << #stmt;                                                              \
            }                                                                              \
        } catch (...) {                                                                    \
            caught_ = true;                                                                \
            ::mini_test::Failure(__FILE__, __LINE__)                                       \
                << "exception of a different type than DomainError in " #stmt;             \
        }                                                                                  \
        if (!caught_) {                                                                    \
            ::mini_test::Failure(__FILE__, __LINE__) << "no DomainError thrown by " #stmt; \
        }                                                                                  \
    } while (0)

#endif  // ATM_TESTS_SUPPORT_DOMAIN_ASSERTIONS_HPP
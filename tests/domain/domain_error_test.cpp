// DomainError: reason() survives, what() contains only the reason name.
#include "support/mini_test.hpp"

#include "support/domain_assertions.hpp"

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

namespace {

    using atm::domain::DomainError;
    using atm::domain::ErrorReason;
    using atm::domain::toString;

    static_assert(noexcept(std::declval<const DomainError&>().reason()));
    static_assert(std::is_base_of_v<std::runtime_error, DomainError>);
    static_assert(std::has_virtual_destructor_v<DomainError>);
    static_assert(std::is_nothrow_copy_constructible_v<DomainError>);
    static_assert(std::is_constructible_v<DomainError, ErrorReason>);
    static_assert(!std::is_convertible_v<ErrorReason, DomainError>);  // constructor is explicit
    static_assert(!std::is_default_constructible_v<DomainError>);     // a reason is mandatory

    TEST(DomainErrorTest, ReasonIsPreservedForEveryReason) {
        for (const ErrorReason reason : atm_test::kAllErrorReasons) {
            const DomainError error(reason);
            EXPECT_EQ(error.reason(), reason);
        }
    }

    TEST(DomainErrorTest, WhatIsPrefixPlusReasonNameOnly) {
        // No PIN, hash, card number or amount can end up in what().
        for (const ErrorReason reason : atm_test::kAllErrorReasons) {
            const DomainError error(reason);
            EXPECT_EQ(std::string(error.what()), "DomainError: " + std::string(toString(reason)));
        }
        EXPECT_EQ(std::string(DomainError(ErrorReason::Overflow).what()), "DomainError: Overflow");
    }

    TEST(DomainErrorTest, CanBeCaughtAsRuntimeErrorAndAsStdException) {
        bool caughtAsRuntimeError = false;
        try {
            throw DomainError(ErrorReason::AtmOutOfCash);
        }
        catch (const std::runtime_error& e) {
            caughtAsRuntimeError = true;
            EXPECT_EQ(std::string(e.what()), "DomainError: AtmOutOfCash");
        }
        EXPECT_TRUE(caughtAsRuntimeError);

        bool caughtAsException = false;
        try {
            throw DomainError(ErrorReason::SamePin);
        }
        catch (const std::exception&) {
            caughtAsException = true;
        }
        EXPECT_TRUE(caughtAsException);
    }

    TEST(DomainErrorTest, CopyKeepsReasonAndMessage) {
        const DomainError original(ErrorReason::DailyLimitExceeded);
        const DomainError copy = original;
        EXPECT_EQ(copy.reason(), ErrorReason::DailyLimitExceeded);
        EXPECT_EQ(std::string(copy.what()), std::string(original.what()));
    }

    TEST(DomainErrorTest, ReasonCanBeReadFromCaughtException) {
        EXPECT_DOMAIN_ERROR(throw DomainError(ErrorReason::InsufficientFunds),
            ErrorReason::InsufficientFunds);
    }

}  // namespace
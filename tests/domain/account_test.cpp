#include "support/mini_test.hpp"
#include "support/domain_assertions.hpp"

#include "domain/account.hpp"
#include "domain/domain_error.hpp"

#include <stdexcept>
#include <utility>

namespace {

using atm::domain::Account;
using atm::domain::CardNumber;
using atm::domain::DomainError;
using atm::domain::ErrorReason;
using atm::domain::Money;

static_assert(noexcept(std::declval<const Account&>().id()));
static_assert(noexcept(std::declval<const Account&>().cardNumber()));
static_assert(noexcept(std::declval<const Account&>().balance()));
static_assert(noexcept(std::declval<const Account&>().isPrimary()));

// --- id & cardNumber & balance & isPrimary ---

TEST(V2Account, ConstructorInitializesFieldsCorrectly) {
    const auto cardNum = CardNumber::parse("4000123456789010");
    const Account acc(101, cardNum, Money::uah(500), true);

    EXPECT_EQ(acc.id(), 101);
    EXPECT_EQ(acc.cardNumber(), cardNum);
    EXPECT_EQ(acc.balance(), Money::uah(500));
    EXPECT_TRUE(acc.isPrimary());
}

TEST(V2Account, ConstructorDefaultsIsPrimaryToFalse) {
    const auto cardNum = CardNumber::parse("4000123456789010");
    const Account acc(1, cardNum, Money::uah(100));

    EXPECT_FALSE(acc.isPrimary());
}

TEST(V2Account, ConstructorRejectsZeroOrNegativeId) {
    const auto cardNum = CardNumber::parse("4000123456789010");

    EXPECT_THROW((void)Account(0, cardNum, Money::uah(100)), std::invalid_argument);
    EXPECT_THROW((void)Account(-5, cardNum, Money::uah(100)), std::invalid_argument);
}

TEST(V2Account, Credit_IncreasesBalance) {
    const auto cardNum = CardNumber::parse("4000123456789010");
    Account acc(1, cardNum, Money::uah(100));

    acc.credit(Money::uah(50));
    EXPECT_EQ(acc.balance(), Money::uah(150));
}

TEST(V2Account, Debit_DecreasesBalance) {
    const auto cardNum = CardNumber::parse("4000123456789010");
    Account acc(1, cardNum, Money::uah(200));

    acc.debit(Money::uah(150));
    EXPECT_EQ(acc.balance(), Money::uah(50));
}

TEST(V2Account, Debit_LeavesZeroBalanceWhenAmountEqualsBalance) {
    const auto cardNum = CardNumber::parse("4000123456789010");
    Account acc(1, cardNum, Money::uah(100));

    acc.debit(Money::uah(100));
    EXPECT_EQ(acc.balance(), Money::uah(0));
}

TEST(V2Account, Debit_ThrowsInsufficientFunds_AmountExceedsBalance) {
    const auto cardNum = CardNumber::parse("4000123456789010");
    Account acc(1, cardNum, Money::uah(100));

    EXPECT_DOMAIN_ERROR(acc.debit(Money::uah(150)), ErrorReason::InsufficientFunds);
    // strong guarantee: balance remains unchanged
    EXPECT_EQ(acc.balance(), Money::uah(100));
}

} // namespace
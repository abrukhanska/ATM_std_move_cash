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
static_assert(noexcept(std::declval<const Account&>().owner()));
static_assert(noexcept(std::declval<const Account&>().balance()));
static_assert(noexcept(std::declval<const Account&>().isPrimary()));

// --- id & cardNumber & balance & isPrimary ---

TEST(V2Account, ConstructorInitializesFieldsCorrectly) {
    const auto cardNumber = CardNumber::parse("4000123456789010");
    const Account account(101, cardNumber, Money::uah(500), true);

    EXPECT_EQ(account.id(), 101);
    EXPECT_EQ(account.owner(), cardNumber);
    EXPECT_EQ(account.balance(), Money::uah(500));
    EXPECT_TRUE(account.isPrimary());
}

TEST(V2Account, ConstructorDefaultsIsPrimaryToFalse) {
    const auto cardNumber = CardNumber::parse("4000123456789010");
    const Account account(1, cardNumber, Money::uah(100));

    EXPECT_FALSE(account.isPrimary());
}

TEST(V2Account, ConstructorRejectsZeroOrNegativeId) {
    const auto cardNumber = CardNumber::parse("4000123456789010");

    EXPECT_THROW((void)Account(0, cardNumber, Money::uah(100)), std::invalid_argument);
    EXPECT_THROW((void)Account(-5, cardNumber, Money::uah(100)), std::invalid_argument);
}

TEST(V2Account, Credit_IncreasesBalance) {
    const auto cardNumber = CardNumber::parse("4000123456789010");
    Account account(1, cardNumber, Money::uah(100));

    account.credit(Money::uah(50));
    EXPECT_EQ(account.balance(), Money::uah(150));
}

TEST(V2Account, Debit_DecreasesBalance) {
    const auto cardNumber = CardNumber::parse("4000123456789010");
    Account account(1, cardNumber, Money::uah(200));

    account.debit(Money::uah(150));
    EXPECT_EQ(account.balance(), Money::uah(50));
}

TEST(V2Account, Debit_LeavesZeroBalanceWhenAmountEqualsBalance) {
    const auto cardNumber = CardNumber::parse("4000123456789010");
    Account account(1, cardNumber, Money::uah(100));

    account.debit(Money::uah(100));
    EXPECT_EQ(account.balance(), Money::uah(0));
}

TEST(V2Account, Debit_ThrowsInsufficientFunds_AmountExceedsBalance) {
    const auto cardNumber = CardNumber::parse("4000123456789010");
    Account account(1, cardNumber, Money::uah(100));

    EXPECT_DOMAIN_ERROR(account.debit(Money::uah(150)), ErrorReason::InsufficientFunds);
    // strong guarantee: balance remains unchanged
    EXPECT_EQ(account.balance(), Money::uah(100));
}

} // namespace
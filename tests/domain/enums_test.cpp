// Enumerations and denomination helpers (specification section 3.2).
#include "support/mini_test.hpp"

#include "support/domain_assertions.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <type_traits>

#include "domain/enums.hpp"

namespace {

    using atm::domain::Denomination;
    using atm::domain::EntryKind;
    using atm::domain::ErrorReason;
    using atm::domain::kAllDenominations;
    using atm::domain::kDenominationCount;
    using atm::domain::toString;

    // Expected test lists contain 15 error reasons and 11 entry kinds.
// The domain defines five supported denominations.
    static_assert(std::size(atm_test::kAllErrorReasons) == 15);
    static_assert(std::size(atm_test::kAllEntryKinds) == 11);
    static_assert(kDenominationCount == 5);

    // Scoped enums: no implicit conversion to int (ST1-style compile-time checks).
    static_assert(!std::is_convertible_v<Denomination, int>);
    static_assert(!std::is_convertible_v<ErrorReason, int>);
    static_assert(!std::is_convertible_v<EntryKind, int>);
    static_assert(!std::is_convertible_v<atm::domain::CardKind, int>);
    static_assert(!std::is_convertible_v<atm::domain::CardStatus, int>);
    static_assert(!std::is_convertible_v<atm::domain::PayeeCategory, int>);

    // The helpers are usable in constant expressions.
    static_assert(atm::domain::denominationUah(Denomination::UAH500) == 500);
    static_assert(toString(ErrorReason::Overflow) == "Overflow");
    static_assert(toString(EntryKind::CashLoaded) == "CashLoaded");

    struct ReasonName {
        ErrorReason reason;
        std::string_view name;
    };
    constexpr ReasonName kReasonNames[] = {
        {ErrorReason::InvalidAmount, "InvalidAmount"},
        {ErrorReason::Overflow, "Overflow"},
        {ErrorReason::InvalidRequisite, "InvalidRequisite"},
        {ErrorReason::UnknownCard, "UnknownCard"},
        {ErrorReason::UnknownAccount, "UnknownAccount"},
        {ErrorReason::UnknownPayee, "UnknownPayee"},
        {ErrorReason::SelfTransfer, "SelfTransfer"},
        {ErrorReason::IncorrectPin, "IncorrectPin"},
        {ErrorReason::SamePin, "SamePin"},
        {ErrorReason::InsufficientFunds, "InsufficientFunds"},
        {ErrorReason::DailyLimitExceeded, "DailyLimitExceeded"},
        {ErrorReason::AtmOutOfCash, "AtmOutOfCash"},
        {ErrorReason::AccessDenied, "AccessDenied"},
        {ErrorReason::InvalidPreview, "InvalidPreview"},
        {ErrorReason::InvalidData, "InvalidData"} };

    struct EntryName {
        EntryKind kind;
        std::string_view name;
    };
    constexpr EntryName kEntryNames[] = {
        {EntryKind::Withdrawal, "Withdrawal"},
        {EntryKind::Deposit, "Deposit"},
        {EntryKind::Transfer, "Transfer"},
        {EntryKind::Payment, "Payment"},
        {EntryKind::PinChange, "PinChange"},
        {EntryKind::CardRetained, "CardRetained"},
        {EntryKind::CardReleased, "CardReleased"},
        {EntryKind::CashLoaded, "CashLoaded"},
        {EntryKind::CashUnloaded, "CashUnloaded"},
        {EntryKind::DeviceInService, "DeviceInService"},
        {EntryKind::DeviceOutOfService, "DeviceOutOfService"} };

    static_assert(std::size(kReasonNames) == std::size(atm_test::kAllErrorReasons));
    static_assert(std::size(kEntryNames) == std::size(atm_test::kAllEntryKinds));

    // --- denominations ------------------------------------------------------------

    TEST(EnumsTest, DenominationValuesMatchSpecification) {
        using atm::domain::denominationUah;
        EXPECT_EQ(denominationUah(Denomination::UAH100), 100);
        EXPECT_EQ(denominationUah(Denomination::UAH200), 200);
        EXPECT_EQ(denominationUah(Denomination::UAH500), 500);
        EXPECT_EQ(denominationUah(Denomination::UAH1000), 1000);
        EXPECT_EQ(denominationUah(Denomination::UAH2000), 2000);
    }

    TEST(EnumsTest, AllDenominationsAreInEnumOrderAndAscending) {
        using atm::domain::denominationIndex;
        using atm::domain::denominationUah;
        EXPECT_EQ(kAllDenominations.size(), std::size_t{ 5 });
        for (std::size_t i = 0; i < kAllDenominations.size(); ++i) {
            EXPECT_EQ(denominationIndex(kAllDenominations[i]), i);
            if (i > 0) {
                EXPECT_TRUE(denominationUah(kAllDenominations[i - 1]) <
                    denominationUah(kAllDenominations[i]));
            }
        }
    }

    TEST(EnumsTest, AllDenominationsAreValidAndEveryValueIsMultipleOfHundred) {
        using atm::domain::denominationUah;
        using atm::domain::isValidDenomination;
        for (const auto d : kAllDenominations) {
            EXPECT_TRUE(isValidDenomination(d));
            EXPECT_EQ(denominationUah(d) % 100, 0);  // withdrawals are multiples of 100 UAH
        }
    }

    TEST(EnumsTest, ValuesOutsideTheEnumerationAreInvalid) {
        using atm::domain::denominationUah;
        using atm::domain::isValidDenomination;
        const Denomination invalid[] = { static_cast<Denomination>(5),
                                        static_cast<Denomination>(99),
                                        static_cast<Denomination>(-1) };
        for (const auto d : invalid) {
            EXPECT_FALSE(isValidDenomination(d));
            EXPECT_EQ(denominationUah(d), 0);  // documented fallback, callers validate first
        }
    }

    // --- names ------------------------------------------------------------------------

    TEST(EnumsTest, ErrorReasonNamesAreExactAndUnique) {
        for (std::size_t i = 0; i < std::size(kReasonNames); ++i) {
            EXPECT_EQ(toString(kReasonNames[i].reason), kReasonNames[i].name);
            for (std::size_t j = i + 1; j < std::size(kReasonNames); ++j) {
                EXPECT_NE(toString(kReasonNames[i].reason), toString(kReasonNames[j].reason));
            }
        }
    }

    TEST(EnumsTest, EntryKindNamesAreExactAndUnique) {
        for (std::size_t i = 0; i < std::size(kEntryNames); ++i) {
            EXPECT_EQ(toString(kEntryNames[i].kind), kEntryNames[i].name);
            for (std::size_t j = i + 1; j < std::size(kEntryNames); ++j) {
                EXPECT_NE(toString(kEntryNames[i].kind), toString(kEntryNames[j].kind));
            }
        }
    }

    TEST(EnumsTest, ValuesOutsideTheEnumerationHaveFallbackName) {
        EXPECT_EQ(toString(static_cast<ErrorReason>(99)), "Unknown");
        EXPECT_EQ(toString(static_cast<EntryKind>(99)), "Unknown");
    }

}  // namespace
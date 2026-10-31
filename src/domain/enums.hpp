#ifndef ATM_DOMAIN_ENUMS_HPP
#define ATM_DOMAIN_ENUMS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace atm::domain {

    enum class CardKind { Client, Service };
    enum class CardStatus { Active, Retained };
    enum class Denomination { UAH100, UAH200, UAH500, UAH1000, UAH2000 };
    enum class PayeeCategory { MobileOperator, Charity };

    enum class EntryKind {
        Withdrawal,
        Deposit,
        Transfer,
        Payment,
        PinChange,
        CardRetained,
        CardReleased,
        CashLoaded,
        CashUnloaded,
        DeviceInService,
        DeviceOutOfService
    };

    enum class ErrorReason {
        InvalidAmount,
        Overflow,
        InvalidRequisite,
        UnknownCard,
        UnknownAccount,
        UnknownPayee,
        SelfTransfer,
        IncorrectPin,
        SamePin,
        InsufficientFunds,
        DailyLimitExceeded,
        AtmOutOfCash,
        AccessDenied,
        InvalidPreview,
        InvalidData
    };

    //  Denominations

    inline constexpr std::size_t kDenominationCount = 5;

    // The order matches the enum values and the indices used by BanknoteSet.
    inline constexpr std::array<Denomination, kDenominationCount> kAllDenominations{
        Denomination::UAH100,
        Denomination::UAH200,
        Denomination::UAH500,
        Denomination::UAH1000,
        Denomination::UAH2000 };

    // Converts the value to an index without checking its range.
    // Check isValidDenomination before accessing an array.
    [[nodiscard]] constexpr std::size_t denominationIndex(Denomination d) noexcept {
        return static_cast<std::size_t>(d);
    }

    // Returns true if the value represents one of the five denominations.
    // Values such as static_cast<Denomination>(999) or static_cast<Denomination>(-1)
    // return false: converting a negative value to size_t produces a large value.
    [[nodiscard]] constexpr bool isValidDenomination(Denomination d) noexcept {
        return denominationIndex(d) < kDenominationCount;
    }

    // Returns the banknote value in whole hryvnias.
    // Call only with a valid denomination (see isValidDenomination).
    // Returns 0 for an invalid value.
    [[nodiscard]] constexpr std::int64_t denominationUah(Denomination d) noexcept {
        switch (d) {
        case Denomination::UAH100: return 100;
        case Denomination::UAH200: return 200;
        case Denomination::UAH500: return 500;
        case Denomination::UAH1000: return 1000;
        case Denomination::UAH2000: return 2000;
        }
        return 0;
    }

    static_assert(denominationIndex(Denomination::UAH2000) == kDenominationCount - 1,
        "останній номінал має останній індекс");
    static_assert(
        [] {
            for (std::size_t i = 0; i < kDenominationCount; ++i) {
                if (denominationIndex(kAllDenominations[i]) != i) {
                    return false;
                }
            }
            return true;
        }(),
            "kAllDenominations має йти в порядку перелічення");
    static_assert(isValidDenomination(Denomination::UAH100) &&
        isValidDenomination(Denomination::UAH2000),
        "крайні номінали валідні");
    static_assert(!isValidDenomination(static_cast<Denomination>(5)) &&
        !isValidDenomination(static_cast<Denomination>(-1)),
        "значення поза переліченням невалідні");

    // Text names for journal entries, tests, and database storage

    [[nodiscard]] constexpr std::string_view toString(ErrorReason r) noexcept {
        switch (r) {
        case ErrorReason::InvalidAmount: return "InvalidAmount";
        case ErrorReason::Overflow: return "Overflow";
        case ErrorReason::InvalidRequisite: return "InvalidRequisite";
        case ErrorReason::UnknownCard: return "UnknownCard";
        case ErrorReason::UnknownAccount: return "UnknownAccount";
        case ErrorReason::UnknownPayee: return "UnknownPayee";
        case ErrorReason::SelfTransfer: return "SelfTransfer";
        case ErrorReason::IncorrectPin: return "IncorrectPin";
        case ErrorReason::SamePin: return "SamePin";
        case ErrorReason::InsufficientFunds: return "InsufficientFunds";
        case ErrorReason::DailyLimitExceeded: return "DailyLimitExceeded";
        case ErrorReason::AtmOutOfCash: return "AtmOutOfCash";
        case ErrorReason::AccessDenied: return "AccessDenied";
        case ErrorReason::InvalidPreview: return "InvalidPreview";
        case ErrorReason::InvalidData: return "InvalidData";
        }
        return "Unknown";
    }

    [[nodiscard]] constexpr std::string_view toString(EntryKind k) noexcept {
        switch (k) {
        case EntryKind::Withdrawal: return "Withdrawal";
        case EntryKind::Deposit: return "Deposit";
        case EntryKind::Transfer: return "Transfer";
        case EntryKind::Payment: return "Payment";
        case EntryKind::PinChange: return "PinChange";
        case EntryKind::CardRetained: return "CardRetained";
        case EntryKind::CardReleased: return "CardReleased";
        case EntryKind::CashLoaded: return "CashLoaded";
        case EntryKind::CashUnloaded: return "CashUnloaded";
        case EntryKind::DeviceInService: return "DeviceInService";
        case EntryKind::DeviceOutOfService: return "DeviceOutOfService";
        }
        return "Unknown";
    }

}  // namespace atm::domain

#endif  // ATM_DOMAIN_ENUMS_HPP
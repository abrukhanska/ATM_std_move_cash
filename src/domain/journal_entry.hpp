#ifndef ATM_DOMAIN_JOURNAL_ENTRY_HPP
#define ATM_DOMAIN_JOURNAL_ENTRY_HPP

#include "enums.hpp"
#include "types.hpp"
#include "card_number.hpp"
#include "banknote_set.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace atm::domain {

    struct JournalEntry {
        JournalId id{0};                                        // Unique identifier of the journal entry.
        std::int64_t atUtc{0};                                  // Timestamp of the entry in UTC, in Unix seconds.
        std::string localDay{};                                 // Local date of the entry in YYYY-MM-DD format.
        EntryKind kind{};                                       // Type of operation or event recorded.
        CardNumber actor{};                                     // Card number of the person who performed the action.
        std::optional<AccountId> account{std::nullopt};         // Client account affected by the entry.
        std::optional<AccountId> target{std::nullopt};          // Destination account for transfers.
        std::optional<CardNumber> subjectCard{std::nullopt};    // Card involved in card retention or release actions.
        Money amount{};                                         // Monetary amount recorded by the entry.
        BanknoteSet cash{};                                     // Physical banknotes included in the transaction.
        std::optional<std::int64_t> payeeId{std::nullopt};      // Identifier of the payment recipient.
        std::string requisite{};                                // Additional payment details or requisites.

        [[nodiscard]] constexpr bool operator==(const JournalEntry&) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const JournalEntry&) const noexcept = default;
    };

    // Verifies that the populated fields are consistent with the entry type.
    [[nodiscard]] inline bool isValidJournalEntry(const JournalEntry& entry) {
        if (entry.localDay.size() != 10) {
            return false;
        }

        const bool hasAccount = entry.account.has_value();
        const bool hasTarget = entry.target.has_value();
        const bool hasSubjectCard = entry.subjectCard.has_value();
        const bool hasPayee = entry.payeeId.has_value();
        const bool hasRequisite = !entry.requisite.empty();

        if (hasAccount && *entry.account <= 0) return false;
        if (hasTarget && *entry.target <= 0) return false;
        if (hasPayee && *entry.payeeId <= 0) return false;

        const Money total = entry.cash.total();
        const bool hasCash = total.kopiykas() > 0;
        const bool hasAmount = entry.amount.kopiykas() > 0;

        switch (entry.kind) {
        case EntryKind::Withdrawal:
        case EntryKind::Deposit:
            return hasAccount && hasAmount && entry.amount == total &&
                !hasTarget && !hasSubjectCard && !hasPayee && !hasRequisite;

        case EntryKind::Transfer:
            return hasAccount && hasTarget && (*entry.account != *entry.target) &&
                hasAmount && !hasCash && !hasSubjectCard && !hasPayee && !hasRequisite;

        case EntryKind::Payment:
            return hasAccount && hasPayee && hasAmount &&
                !hasTarget && !hasSubjectCard && !hasCash;

        case EntryKind::CashLoaded:
        case EntryKind::CashUnloaded:
            return hasAmount && entry.amount == total &&
                !hasAccount && !hasTarget && !hasSubjectCard && !hasPayee && !hasRequisite;

        case EntryKind::CardRetained:
        case EntryKind::CardReleased:
            return hasSubjectCard && !hasAmount && !hasCash &&
                !hasAccount && !hasTarget && !hasPayee && !hasRequisite;

        case EntryKind::PinChange:
        case EntryKind::DeviceInService:
        case EntryKind::DeviceOutOfService:
            return !hasAmount && !hasCash && !hasAccount &&
                !hasTarget && !hasSubjectCard && !hasPayee && !hasRequisite;
        }

        return false;
    }

} // namespace atm::domain

#endif // ATM_DOMAIN_JOURNAL_ENTRY_HPP
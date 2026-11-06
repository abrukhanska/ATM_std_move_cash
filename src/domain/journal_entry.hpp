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
        JournalId id{0};
        std::int64_t atUtc{0};
        std::string localDay{};
        EntryKind kind{};
        CardNumber actor{};
        std::optional<AccountId> account{std::nullopt};
        std::optional<AccountId> target{std::nullopt};
        std::optional<CardNumber> subjectCard{std::nullopt};
        Money amount{};
        BanknoteSet cash{};
        std::optional<std::int64_t> payeeId{std::nullopt};
        std::string requisite{};

        [[nodiscard]] constexpr bool operator==(const JournalEntry&) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const JournalEntry&) const noexcept = default;
    };

} // namespace atm::domain

#endif // ATM_DOMAIN_JOURNAL_ENTRY_HPP
#ifndef ATM_DOMAIN_IJOURNAL_REPOSITORY_HPP
#define ATM_DOMAIN_IJOURNAL_REPOSITORY_HPP

#include "domain/journal_entry.hpp"

namespace atm::domain {

    class IJournalRepository {
    public:
        virtual ~IJournalRepository() noexcept = default;

        virtual JournalEntry append(const JournalEntry& entry) = 0;
        [[nodiscard]] virtual std::vector<JournalEntry> last(std::size_t count) const = 0;
        [[nodiscard]] virtual Money withdrawnOn(const CardNumber& cardNumber, const std::string& day) const = 0;
        [[nodiscard]] virtual Money sumAmount(EntryKind kind) const = 0;
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_IJOURNAL_REPOSITORY_HPP
#ifndef ATM_INFRASTRUCTURE_IN_MEMORY_JOURNAL_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_IN_MEMORY_JOURNAL_REPOSITORY_HPP

#include "domain/repositories.hpp"
#include <algorithm>
#include <vector>

namespace atm::infrastructure {

class InMemoryJournalRepository : public domain::IJournalRepository {
public:
    domain::JournalEntry append(const domain::JournalEntry& entry) override {
        domain::JournalEntry saved = entry;
        saved.id = nextId_++;
        entries_.push_back(saved);
        return saved;
    }

    [[nodiscard]] std::vector<domain::JournalEntry> last(std::size_t count) const override {
        std::vector<domain::JournalEntry> result;
        const std::size_t n = std::min(count, entries_.size());
        for (std::size_t i = 0; i < n; ++i) {
            result.push_back(entries_[entries_.size() - 1 - i]);
        }
        return result;
    }

    [[nodiscard]] domain::Money withdrawnOn(const domain::CardNumber& cardNumber, const std::string& day) const override {
        domain::Money total{};
        for (const auto& entry : entries_) {
            if (entry.actor == cardNumber && entry.localDay == day && entry.kind == domain::EntryKind::Withdrawal) {
                total = total + entry.amount;
            }
        }
        return total;
    }

    [[nodiscard]] domain::Money sumAmount(domain::EntryKind kind) const override {
        domain::Money total{};
        for (const auto& entry : entries_) {
            if (entry.kind == kind) {
                total = total + entry.amount;
            }
        }
        return total;
    }

private:
    domain::JournalId nextId_{1};
    std::vector<domain::JournalEntry> entries_;
};

} // namespace atm::infrastructure

#endif // ATM_INFRASTRUCTURE_IN_MEMORY_JOURNAL_REPOSITORY_HPP
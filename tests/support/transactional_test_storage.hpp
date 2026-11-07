#ifndef ATM_TEST_SUPPORT_TRANSACTIONAL_TEST_STORAGE_HPP
#define ATM_TEST_SUPPORT_TRANSACTIONAL_TEST_STORAGE_HPP

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "domain/istorage.hpp"
#include "infrastructure/in_memory_account_repository.hpp"
#include "infrastructure/in_memory_card_repository.hpp"
#include "infrastructure/in_memory_cash_repository.hpp"
#include "infrastructure/in_memory_journal_repository.hpp"

namespace atm::test_support {

    class InjectedStorageFailure final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class TestStorage final : public domain::IStorage, private domain::IJournalRepository {
    public:
        enum class Failure { None, Append, Commit };

        Failure failure = Failure::None;
        std::optional<domain::Money> withdrawnTodayOverride;
        int commits = 0;
        int rollbacks = 0;

        domain::ICardRepository& cards() override { return data_.cards; }
        domain::IAccountRepository& accounts() override { return data_.accounts; }
        domain::ICashRepository& cash() override { return data_.cash; }
        domain::IJournalRepository& journal() override { return *this; }

        void begin() override {
            if (before_) {
                throw std::logic_error("Unexpected nested transaction");
            }
            before_.emplace(data_);
        }

        void commit() override {
            if (failure == Failure::Commit) {
                throw InjectedStorageFailure("Injected commit failure");
            }
            before_.reset();
            ++commits;
        }

        void rollback() noexcept override {
            if (before_) {
                data_ = std::move(*before_);
                before_.reset();
                ++rollbacks;
            }
        }

        bool broken() const noexcept override { return false; }

    private:
        domain::JournalEntry append(const domain::JournalEntry& entry) override {
            auto saved = data_.journal.append(entry);
            if (failure == Failure::Append) {
                throw InjectedStorageFailure("Injected append failure after write");
            }
            return saved;
        }

        std::vector<domain::JournalEntry> last(std::size_t count) const override {
            return data_.journal.last(count);
        }

        domain::Money withdrawnOn(const domain::CardNumber& card, const std::string& day) const override {
            if (withdrawnTodayOverride) {
                return *withdrawnTodayOverride;
            }
            return data_.journal.withdrawnOn(card, day);
        }

        domain::Money sumAmount(domain::EntryKind kind) const override {
            return data_.journal.sumAmount(kind);
        }

        struct State {
            atm::infrastructure::InMemoryCardRepository cards;
            atm::infrastructure::InMemoryAccountRepository accounts;
            atm::infrastructure::InMemoryCashRepository cash;
            atm::infrastructure::InMemoryJournalRepository journal;
        };

        static_assert(std::is_nothrow_move_assignable_v<State>);
        State data_;
        std::optional<State> before_;
    };

}  // namespace atm::test_support

#endif  // ATM_TEST_SUPPORT_TRANSACTIONAL_TEST_STORAGE_HPP

#ifndef ATM_TEST_SUPPORT_TRANSACTIONAL_TEST_STORAGE_HPP
#define ATM_TEST_SUPPORT_TRANSACTIONAL_TEST_STORAGE_HPP

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "domain/istorage.hpp"

namespace atm::test_support {

    class InjectedStorageFailure final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class TestStorage final : public domain::IStorage, private domain::IJournalRepository {
    private:
        struct State {
            std::vector<domain::Card> cards;
            std::vector<domain::Account> accounts;
            domain::BanknoteSet cash;
            std::vector<domain::JournalEntry> journal;
        };

        class Cards final : public domain::ICardRepository {
        public:
            explicit Cards(TestStorage& owner) : owner_(owner) {}
            std::optional<domain::Card> find(const domain::CardNumber& number) const override {
                for (const auto& card : owner_.data_.cards) {
                    if (card.number() == number) return card;
                }
                return std::nullopt;
            }
            std::vector<domain::Card> listRetained() const override {
                std::vector<domain::Card> result;
                for (const auto& card : owner_.data_.cards) {
                    if (card.status() == domain::CardStatus::Retained) result.push_back(card);
                }
                return result;
            }
            void save(const domain::Card& card) override {
                owner_.requireTransaction();
                for (auto& current : owner_.data_.cards) {
                    if (current.number() == card.number()) {
                        current = card;
                        owner_.afterSave();
                        return;
                    }
                }
                throw std::logic_error("Cannot save a missing test card");
            }
        private:
            TestStorage& owner_;
        };

        class Accounts final : public domain::IAccountRepository {
        public:
            explicit Accounts(TestStorage& owner) : owner_(owner) {}
            std::optional<domain::Account> find(domain::AccountId id) const override {
                for (const auto& account : owner_.data_.accounts) {
                    if (account.id() == id) return account;
                }
                return std::nullopt;
            }
            std::optional<domain::Account> findPrimary(const domain::CardNumber& card) const override {
                for (const auto& account : owner_.data_.accounts) {
                    if (account.owner() == card && account.isPrimary()) return account;
                }
                return std::nullopt;
            }
            std::vector<domain::Account> listByCard(const domain::CardNumber& card) const override {
                std::vector<domain::Account> result;
                for (const auto& account : owner_.data_.accounts) {
                    if (account.owner() == card) result.push_back(account);
                }
                return result;
            }
            void save(const domain::Account& account) override {
                owner_.requireTransaction();
                for (auto& current : owner_.data_.accounts) {
                    if (current.id() == account.id()) {
                        current = account;
                        owner_.afterSave();
                        return;
                    }
                }
                throw std::logic_error("Cannot save a missing test account");
            }
        private:
            TestStorage& owner_;
        };

        class Cash final : public domain::ICashRepository {
        public:
            explicit Cash(TestStorage& owner) : owner_(owner) {}
            domain::BanknoteSet load() const override { return owner_.data_.cash; }
            void save(const domain::BanknoteSet& notes) override {
                owner_.requireTransaction();
                owner_.data_.cash = notes;
                owner_.afterSave();
            }
        private:
            TestStorage& owner_;
        };

    public:
        enum class Failure { None, Append, Commit };
        Failure failure = Failure::None;
        std::optional<domain::Money> withdrawnTodayOverride;
        std::optional<std::size_t> failAfterSave;
        int commits = 0;
        int rollbacks = 0;

        TestStorage() : cards_(*this), accounts_(*this), cash_(*this) {}
        TestStorage(const TestStorage&) = delete;
        TestStorage& operator=(const TestStorage&) = delete;
        TestStorage(TestStorage&&) = delete;
        TestStorage& operator=(TestStorage&&) = delete;

        domain::ICardRepository& cards() override { return cards_; }
        domain::IAccountRepository& accounts() override { return accounts_; }
        domain::ICashRepository& cash() override { return cash_; }
        domain::IJournalRepository& journal() override { return *this; }

        void seed(const domain::Card& card) {
            requireFixtureSetup();
            for (auto& current : data_.cards) {
                if (current.number() == card.number()) { current = card; return; }
            }
            data_.cards.push_back(card);
        }
        void seed(const domain::Account& account) {
            requireFixtureSetup();
            for (auto& current : data_.accounts) {
                if (current.id() == account.id()) { current = account; return; }
            }
            data_.accounts.push_back(account);
        }
        void seed(const domain::BanknoteSet& notes) {
            requireFixtureSetup();
            data_.cash = notes;
        }
        domain::JournalEntry seed(const domain::JournalEntry& entry) {
            requireFixtureSetup();
            return storeEntry(entry);
        }

        void begin() override {
            if (before_) throw std::logic_error("Unexpected nested transaction");
            before_.emplace(data_);
            saves_ = 0;
        }
        void commit() override {
            requireTransaction();
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
        void requireTransaction() const {
            if (!before_) throw std::logic_error("Test write outside a transaction");
        }
        void requireFixtureSetup() const {
            if (before_) throw std::logic_error("Cannot seed during a transaction");
        }
        void afterSave() {
            ++saves_;
            if (failAfterSave && saves_ == *failAfterSave) {
                throw InjectedStorageFailure("Injected save failure after write");
            }
        }
        domain::JournalEntry storeEntry(const domain::JournalEntry& entry) {
            auto saved = entry;
            if (!data_.journal.empty() && data_.journal.back().id ==
                (std::numeric_limits<domain::JournalId>::max)()) {
                throw std::overflow_error("Test journal id overflow");
            }
            saved.id = data_.journal.empty() ? 1 : data_.journal.back().id + 1;
            data_.journal.push_back(saved);
            return saved;
        }
        domain::JournalEntry append(const domain::JournalEntry& entry) override {
            requireTransaction();
            auto saved = storeEntry(entry);
            if (failure == Failure::Append) {
                throw InjectedStorageFailure("Injected append failure after write");
            }
            return saved;
        }
        std::vector<domain::JournalEntry> last(std::size_t count) const override {
            std::vector<domain::JournalEntry> result;
            for (auto it = data_.journal.rbegin(); it != data_.journal.rend() && result.size() < count; ++it) {
                result.push_back(*it);
            }
            return result;
        }
        domain::Money withdrawnOn(const domain::CardNumber& card, const std::string& day) const override {
            if (withdrawnTodayOverride) return *withdrawnTodayOverride;
            domain::Money sum;
            for (const auto& entry : data_.journal) {
                if (entry.kind == domain::EntryKind::Withdrawal && entry.actor == card && entry.localDay == day) {
                    sum = sum + entry.amount;
                }
            }
            return sum;
        }
        domain::Money sumAmount(domain::EntryKind kind) const override {
            domain::Money sum;
            for (const auto& entry : data_.journal) {
                if (entry.kind == kind) sum = sum + entry.amount;
            }
            return sum;
        }

        static_assert(std::is_nothrow_move_assignable_v<State>);
        State data_;
        std::optional<State> before_;
        std::size_t saves_ = 0;
        Cards cards_;
        Accounts accounts_;
        Cash cash_;
    };

}  // namespace atm::test_support

#endif  // ATM_TEST_SUPPORT_TRANSACTIONAL_TEST_STORAGE_HPP

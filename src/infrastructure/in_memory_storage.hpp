#ifndef ATM_INFRASTRUCTURE_IN_MEMORY_STORAGE_HPP
#define ATM_INFRASTRUCTURE_IN_MEMORY_STORAGE_HPP

#include "domain/istorage.hpp"
#include "in_memory_account_repository.hpp"
#include "in_memory_card_repository.hpp"
#include "in_memory_cash_repository.hpp"
#include "in_memory_journal_repository.hpp"

namespace atm::infrastructure {

class InMemoryStorage : public domain::IStorage {
public:
    [[nodiscard]] domain::ICardRepository& cards() override { return cardsRepo_; }
    [[nodiscard]] domain::IAccountRepository& accounts() override { return accountsRepo_; }
    [[nodiscard]] domain::ICashRepository& cash() override { return cashRepo_; }
    [[nodiscard]] domain::IJournalRepository& journal() override { return journalRepo_; }

    void begin() override { inTransaction_ = true; }
    void commit() override { inTransaction_ = false; }
    void rollback() noexcept override { inTransaction_ = false; }
    [[nodiscard]] bool broken() const noexcept override { return false; }

private:
    InMemoryCardRepository cardsRepo_;
    InMemoryAccountRepository accountsRepo_;
    InMemoryCashRepository cashRepo_;
    InMemoryJournalRepository journalRepo_;
    bool inTransaction_{false};
};

} // namespace atm::infrastructure

#endif // ATM_INFRASTRUCTURE_IN_MEMORY_STORAGE_HPP
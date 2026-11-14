#ifndef ATM_INFRASTRUCTURE_SQL_DATABASE_HPP
#define ATM_INFRASTRUCTURE_SQL_DATABASE_HPP

#include "domain/istorage.hpp"

#include "card_repository.hpp"
#include "account_repository.hpp"
#include "cash_repository.hpp"
#include "journal_repository.hpp"

struct sqlite3;

namespace atm::infrastructure::sql {

    // RAII Closer for SQLite connection handle
    // closes DB on destruction or init failure
    struct SqliteCloser { void operator()(sqlite3* db) const noexcept; };

    class Database : public domain::IStorage {
    public:
        Database(const std::string&); // passes the path to the database file

        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;

        ~Database() noexcept override = default;

        domain::ICardRepository& cards() override { return cards_; }
        domain::IAccountRepository& accounts() override { return accounts_; }
        domain::ICashRepository& cash() override { return cash_; }
        domain::IJournalRepository& journal() override { return journal_; }
        
        void begin() override;
        void commit() override;
        void rollback() noexcept override;

        [[nodiscard]] sqlite3* handle() const;
        bool broken() const noexcept override { return broken_; }

    private:
        CardRepository cards_;
        AccountRepository accounts_;
        CashRepository cash_;
        JournalRepository journal_;

        std::unique_ptr<sqlite3, SqliteCloser> handle_{nullptr};
        bool broken_{false};

        void init();
    };

} // namespace atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_SQL_DATABASE_HPP

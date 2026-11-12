#ifndef ATM_INFRASTRUCTURE_SQL_CASH_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_SQL_CASH_REPOSITORY_HPP

#include "domain/icash_repository.hpp"

namespace atm::infrastructure::sql {

    class Database;

    class CashRepository : public domain::ICashRepository {
    public:
        explicit CashRepository(Database& db) : db_(db) {}
        
        [[nodiscard]] domain::BanknoteSet load() const override;
        void save(const domain::BanknoteSet& banknotes) override;
        
    private:
        Database& db_;
    };

} // atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_SQL_CASH_REPOSITORY_HPP
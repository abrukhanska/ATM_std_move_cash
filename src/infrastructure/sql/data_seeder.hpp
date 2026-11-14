#ifndef ATM_INFRASTRUCTURE_SQL_DATA_SEEDER_HPP
#define ATM_INFRASTRUCTURE_SQL_DATA_SEEDER_HPP

#include "infrastructure/sql/database.hpp"

namespace atm::infrastructure::sql {

    void seedDemoDataIfEmpty(Database&, const std::vector<domain::Card>&, const std::vector<domain::Account>&);
    void validateDatabase(Database& db);

} // namespace atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_SQL_SEEDER_HPP
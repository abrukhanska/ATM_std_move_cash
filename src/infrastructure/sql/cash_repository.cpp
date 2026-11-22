#include "infrastructure/sql/cash_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"

#include <sqlite3.h>
#include "infrastructure/sql/stmt.hpp"

namespace atm::infrastructure::sql {

    domain::BanknoteSet CashRepository::load() const
    {
        sqlite3* handle = db_.handle();
        const char* sql = "SELECT denomination, count FROM banknotes;";
        Stmt stmt = prepare(handle, sql, "CashRepository::load prepare");

        domain::BanknoteSet banknotes;

        int rc = SQLITE_OK;
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            std::int64_t denomination = sqlite3_column_int64(stmt.get(), 0);

            std::uint32_t count = [&]() {
                std::int64_t c = sqlite3_column_int64(stmt.get(), 1);
                if (c < 0 || c > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
                    throw StorageError("CashRepository::load", SQLITE_RANGE, "Banknote count out of range");
                }
                return static_cast<std::uint32_t>(c);
            }();

            bool matched = false;
            for (const auto& d : domain::kAllDenominations) {
                if (domain::denominationUah(d) == denomination) {
                    banknotes = banknotes.with(d, count);
                    matched = true;
                    break;
                }
            }
            if (!matched) throw StorageError("CashRepository::load", SQLITE_CORRUPT, "Unknown banknote denomination");
        }
        if (rc != SQLITE_DONE) throw StorageError("CashRepository::load step", rc, sqlite3_errmsg(handle));

        return banknotes;
    }

    void CashRepository::save(const domain::BanknoteSet &banknotes)
    {
        sqlite3* handle = db_.handle();
        const char* sql = "INSERT OR REPLACE INTO banknotes (denomination, count) VALUES (?, ?);";
        Stmt stmt = prepare(handle, sql, "CashRepository::save prepare");

        for (const auto& d : domain::kAllDenominations) {
            std::int64_t denomination = domain::denominationUah(d);
            std::uint32_t count = banknotes.count(d);

            // reset the statement to its initial state to allow re-execution
            sqlite3_reset(stmt.get());
            // clear previous parameter bindings before the next loop iteration
            sqlite3_clear_bindings(stmt.get());

            if (sqlite3_bind_int64(stmt.get(), 1, denomination) != SQLITE_OK ||
                sqlite3_bind_int64(stmt.get(), 2, static_cast<std::int64_t>(count)) != SQLITE_OK) {
                throw StorageError("CashRepository::save bind", SQLITE_ERROR, sqlite3_errmsg(handle));
            }

            int rc = sqlite3_step(stmt.get());
            if (rc != SQLITE_DONE) throw StorageError("CashRepository::save step", rc, sqlite3_errmsg(handle));
        }
    }
    
} // atm::infrastructure::sql

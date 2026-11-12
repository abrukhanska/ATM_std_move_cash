#include <sqlite3.h>

#include "infrastructure/sql/cash_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"

namespace atm::infrastructure::sql {

    struct SqliteStmtDeleter {
        void operator()(sqlite3_stmt* stmt) const noexcept {
            sqlite3_finalize(stmt);
        }
    };
    using Stmt = std::unique_ptr<sqlite3_stmt, SqliteStmtDeleter>;

    domain::BanknoteSet CashRepository::load() const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT denomination, count FROM banknotes;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if(rc != SQLITE_OK) {
            throw StorageError("CashRepository::load prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);
        domain::BanknoteSet banknotes;

        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            std::int64_t db_denomination = sqlite3_column_int64(stmt.get(), 0); // column denomination
            std::uint32_t db_count = static_cast<std::uint32_t>(sqlite3_column_int(stmt.get(), 1)); // column count

            for (const auto& denomination : domain::kAllDenominations) {
                if (domain::denominationUah(denomination) == db_denomination) {
                    banknotes = banknotes.with(denomination, db_count);
                    break;
                }
            }
        }

        return banknotes;
    }

    void CashRepository::save(const domain::BanknoteSet &banknotes)
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "INSERT OR REPLACE INTO banknotes (denomination, count) VALUES (?, ?);";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("CashRepository::save prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);

        for (const auto& denomination : domain::kAllDenominations) {
            std::int64_t denomination_val = domain::denominationUah(denomination); // column denomination
            std::uint32_t count_val = banknotes.count(denomination); // column count

            // reset the statement to its initial state to allow re-execution
            sqlite3_reset(stmt.get());
            // clear previous parameter bindings before the next loop iteration
            sqlite3_clear_bindings(stmt.get());

            // bind parameters to the placeholders (?) and ensure the operation succeeds
            if (sqlite3_bind_int64(stmt.get(), 1, denomination_val) != SQLITE_OK ||
                sqlite3_bind_int(stmt.get(), 2, static_cast<int>(count_val)) != SQLITE_OK) {
                throw StorageError("CashRepository::save bind", SQLITE_ERROR, sqlite3_errmsg(handle));
            }

            rc = sqlite3_step(stmt.get());
            if (rc != SQLITE_DONE) {
                throw StorageError("CashRepository::save step", rc, sqlite3_errmsg(handle));
            }
        }
    }
    
} // atm::infrastructure::sql

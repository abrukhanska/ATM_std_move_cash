#ifndef ATM_INFRASTRUCTURE_SQL_STMT_HPP
#define ATM_INFRASTRUCTURE_SQL_STMT_HPP

#include "infrastructure/sql/storage_error.hpp"

#include <sqlite3.h>
#include <memory>
#include <string_view>

namespace atm::infrastructure::sql {

    struct StmtFinalizer {
        void operator()(sqlite3_stmt* stmt) const noexcept {
            if (stmt) {
                sqlite3_finalize(stmt);
            }
        }
    };
    using Stmt = std::unique_ptr<sqlite3_stmt, StmtFinalizer>;

    [[nodiscard]] inline Stmt prepare(sqlite3* handle, const char* sql, std::string_view context) {
        sqlite3_stmt* stmt_ptr = nullptr;
        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError(std::string(context), rc, sqlite3_errmsg(handle));
        }
        return Stmt(stmt_ptr);
    }

} // namespace atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_SQL_STMT_HPP
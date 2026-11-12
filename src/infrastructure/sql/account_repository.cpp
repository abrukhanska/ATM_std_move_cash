#include <sqlite3.h>

#include "infrastructure/sql/account_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"

namespace atm::infrastructure::sql {

    struct SqliteStmtDeleter {
        void operator()(sqlite3_stmt* stmt) const noexcept {
            if (stmt) {
                sqlite3_finalize(stmt);
            }
        }
    };
    using Stmt = std::unique_ptr<sqlite3_stmt, SqliteStmtDeleter>;

    namespace {
        domain::Account extractAccount(sqlite3_stmt* stmt) {
            domain::AccountId id = sqlite3_column_int64(stmt, 0);
            // using reinterpret_cast to cast from const unsigned char* to const char*,
            // since the C++ compiler refuses to do it automatically or via static_cast.
            const char* card_number_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string card_number = card_number_text ? card_number_text : "";
            std::int64_t balance_kop = sqlite3_column_int64(stmt, 2);
            bool is_primary = sqlite3_column_int(stmt, 3) != 0;

            return domain::Account(
                id,
                domain::CardNumber::parse(card_number),
                domain::Money::ofKopiykas(balance_kop),
                is_primary
            );
        }
    }

    std::optional<domain::Account> AccountRepository::find(domain::AccountId id) const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT account_id, card_number, balance_kop, is_primary FROM accounts WHERE account_id = ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("AccountRepository::find prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);

        if (sqlite3_bind_int64(stmt.get(), 1, id) != SQLITE_OK) {
            throw StorageError("AccountRepository::find bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) return extractAccount(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::find step", rc, sqlite3_errmsg(handle));

        return std::nullopt;
    }

    std::optional<domain::Account> AccountRepository::findPrimary(const domain::CardNumber &number) const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT account_id, card_number, balance_kop, is_primary FROM accounts WHERE card_number = ? AND is_primary = 1;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("AccountRepository::findPrimary prepare", rc, sqlite3_errmsg(handle));
        }
        
        Stmt stmt(stmt_ptr);
        std::string card_number = number.str();
    
        if (sqlite3_bind_text(stmt.get(), 1, card_number.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("AccountRepository::findPrimary bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) return extractAccount(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::findPrimary step", rc, sqlite3_errmsg(handle));

        return std::nullopt;
    }

   std::vector<domain::Account> AccountRepository::listByCard(const domain::CardNumber &number) const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT account_id, card_number, balance_kop, is_primary FROM accounts WHERE card_number = ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("AccountRepository::listByCard prepare", rc, sqlite3_errmsg(handle));
        }
        
        Stmt stmt(stmt_ptr);
        std::string card_number = number.str();

        if (sqlite3_bind_text(stmt.get(), 1, card_number.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("AccountRepository::listByCard bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        std::vector<domain::Account> accounts;
        
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            accounts.push_back(extractAccount(stmt.get()));
        }
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::listByCard step", rc, sqlite3_errmsg(handle));

        return accounts;
    }

    void AccountRepository::save(const domain::Account &account)
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "INSERT OR REPLACE INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES (?, ?, ?, ?);";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("AccountRepository::save prepare", rc, sqlite3_errmsg(handle));
        }
        
        Stmt stmt(stmt_ptr);

        domain::AccountId account_id = account.id();
        std::string card_number = account.owner().str();
        std::int64_t balance_kop = account.balance().kopiykas();
        int is_primary = account.isPrimary() ? 1 : 0;

        if (sqlite3_bind_int64(stmt.get(), 1, account_id) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 2, card_number.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 3, balance_kop) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(), 4, is_primary) != SQLITE_OK) {
            
            throw StorageError("AccountRepository::save bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::save step", rc, sqlite3_errmsg(handle));
    }

} // atm::infrastructure::sql
#include "infrastructure/sql/account_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "infrastructure/sql/stmt.hpp"

#include <sqlite3.h>

namespace atm::infrastructure::sql {

    namespace {
        domain::Account extractAccount(sqlite3_stmt* stmt) {
            domain::AccountId id = sqlite3_column_int64(stmt, 0);
            if (id <= 0) throw StorageError("AccountRepository::extractAccount", SQLITE_CORRUPT, "Invalid account_id");

            // using reinterpret_cast to cast from const unsigned char* to const char*,
            // since the C++ compiler refuses to do it automatically or via static_cast.
            domain::CardNumber card_number = [&]() {
                const auto* card_number_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (!card_number_ptr) {
                    throw StorageError("AccountRepository::extractAccount", SQLITE_CORRUPT, "Null card_number");
                }
                int card_number_length = sqlite3_column_bytes(stmt, 1);
                try {
                    return domain::CardNumber::parse(std::string_view(card_number_ptr, static_cast<std::size_t>(card_number_length)));
                } catch (const std::invalid_argument&) {
                    throw StorageError("AccountRepository::extractAccount", SQLITE_CORRUPT, "Invalid card_number");
                }
            }();
            
            domain::Money balance_kop = [&]() {
                try {
                    return domain::Money::ofKopiykas(sqlite3_column_int64(stmt, 2));
                } catch (const domain::DomainError&) {
                    throw StorageError("AccountRepository::extractAccount", SQLITE_CORRUPT, "Invalid balance_kop");
                }
            }();
            
            bool is_primary = [&]() {
                std::int64_t is_primary_int64 = sqlite3_column_int64(stmt, 3);
                if (is_primary_int64 != 0 && is_primary_int64 != 1) {
                    throw StorageError("AccountRepository::extractAccount", SQLITE_CORRUPT, "Invalid is_primary");
                }
                return is_primary_int64 == 1;
            }();

            return domain::Account(
                id,
                card_number,
                balance_kop,
                is_primary
            );
        }
    }

    std::optional<domain::Account> AccountRepository::find(domain::AccountId id) const
    {
        sqlite3* handle = db_.handle();
        const char* sql = "SELECT account_id, card_number, balance_kop, is_primary FROM accounts WHERE account_id = ?;";
        Stmt stmt = prepare(handle, sql, "AccountRepository::find prepare");

        if (sqlite3_bind_int64(stmt.get(), 1, id) != SQLITE_OK) {
            throw StorageError("AccountRepository::find bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) return extractAccount(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::find step", rc, sqlite3_errmsg(handle));

        return std::nullopt;
    }

    std::optional<domain::Account> AccountRepository::findPrimary(const domain::CardNumber &number) const
    {
        sqlite3* handle = db_.handle();
        const char* sql = "SELECT account_id, card_number, balance_kop, is_primary FROM accounts WHERE card_number = ? AND is_primary = 1;";
        Stmt stmt = prepare(handle, sql, "AccountRepository::findPrimary prepare");

        std::string card_number = number.str();
        if (sqlite3_bind_text(stmt.get(), 1, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("AccountRepository::findPrimary bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) return extractAccount(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::findPrimary step", rc, sqlite3_errmsg(handle));

        return std::nullopt;
    }

    std::vector<domain::Account> AccountRepository::listByCard(const domain::CardNumber &number) const
    {
        sqlite3* handle = db_.handle();
        const char* sql = "SELECT account_id, card_number, balance_kop, is_primary FROM accounts WHERE card_number = ? ORDER BY account_id ASC;";
        Stmt stmt = prepare(handle, sql, "AccountRepository::listByCard prepare");

        std::string card_number = number.str();
        if (sqlite3_bind_text(stmt.get(), 1, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("AccountRepository::listByCard bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        std::vector<domain::Account> accounts;
        
        int rc = SQLITE_OK;
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            accounts.push_back(extractAccount(stmt.get()));
        }
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::listByCard step", rc, sqlite3_errmsg(handle));

        return accounts;
    }

    void AccountRepository::save(const domain::Account &account)
    {
        sqlite3* handle = db_.handle();
        const char* sql = "UPDATE accounts SET card_number = ?, balance_kop = ?, is_primary = ? WHERE account_id = ?;";
        Stmt stmt = prepare(handle, sql, "AccountRepository::save prepare");

        std::string card_number = account.owner().str();
        std::int64_t balance_kop = account.balance().kopiykas();
        int is_primary = account.isPrimary() ? 1 : 0;
        domain::AccountId account_id = account.id();

        if (sqlite3_bind_text(stmt.get(), 1, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 2, balance_kop) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(), 3, is_primary) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 4, account_id) != SQLITE_OK) {
            
            throw StorageError("AccountRepository::save bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        int rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("AccountRepository::save step", rc, sqlite3_errmsg(handle));
        if (sqlite3_changes(handle) != 1) throw StorageError("AccountRepository::save step", SQLITE_NOTFOUND, "Account not found");
    }

} // atm::infrastructure::sql
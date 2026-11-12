#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"

#include <sqlite3.h>

namespace atm::infrastructure::sql {

    void SqliteDeleter::operator()(sqlite3 *db) const noexcept { if (db) sqlite3_close(db); }

    Database::Database(const std::string& path) : cards_(*this), accounts_(*this), cash_(*this), journal_(*this)
    {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(path.c_str(), &db);
        handle_.reset(db);

        if(rc != SQLITE_OK) {
            std::string reason = db ? sqlite3_errmsg(db) : "Sqlite handle memory allocation error";
            broken_ = true;
            throw StorageError("sqlite3_open", rc, reason);
        }

        init();
    }

    void Database::begin()
    {
        char* error_message = nullptr;
        int rc = sqlite3_exec(handle_.get(), "BEGIN;", nullptr, nullptr, &error_message);

        if (rc != SQLITE_OK) {
            std::string message = error_message ? error_message : "Begin transaction error";
            if(error_message) sqlite3_free(error_message);
            throw StorageError("begin", rc , message);
        }
    }

    void Database::commit()
    {
        char* error_message = nullptr;
        int rc = sqlite3_exec(handle_.get(), "COMMIT;", nullptr, nullptr, &error_message);

        if(rc != SQLITE_OK) {
            std::string message = error_message ? error_message : "Commit transaction error";
            if(error_message) sqlite3_free(error_message);
            throw StorageError("commit", rc , message);
        }
    }

    void Database::rollback() noexcept
    {
        char* error_message = nullptr;
        int rc = sqlite3_exec(handle_.get(), "ROLLBACK;", nullptr, nullptr, &error_message);

        if(rc != SQLITE_OK) {
            if(error_message) sqlite3_free(error_message);
            broken_ = true;
        }
    }

    void Database::init()
    {
        const char* sql = R"(
            PRAGMA foreign_keys = ON;

            CREATE TABLE IF NOT EXISTS cards (
                card_number TEXT PRIMARY KEY,
                holder_name TEXT NOT NULL,
                pin_hash TEXT NOT NULL,
                kind TEXT NOT NULL,
                status TEXT NOT NULL,
                failed_attempts INTEGER NOT NULL DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS accounts (
                account_id INTEGER PRIMARY KEY,
                card_number TEXT NOT NULL,
                balance_kop INTEGER NOT NULL DEFAULT 0,
                is_primary INTEGER NOT NULL DEFAULT 0,
                FOREIGN KEY(card_number) REFERENCES cards(card_number)
            );

            CREATE UNIQUE INDEX IF NOT EXISTS account_ids_is_primary 
            ON accounts(card_number) WHERE is_primary = 1;

            CREATE TABLE IF NOT EXISTS banknotes (
                denomination INTEGER PRIMARY KEY,
                count INTEGER NOT NULL DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS journal (
                journal_id INTEGER PRIMARY KEY AUTOINCREMENT,
                at_utc INTEGER NOT NULL,
                local_day TEXT NOT NULL,
                kind TEXT NOT NULL,
                actor_card TEXT NOT NULL,
                account_id INTEGER,
                target_account_id INTEGER,
                subject_card TEXT,
                amount_kop INTEGER NOT NULL DEFAULT 0,
                    
                cash_100 INTEGER NOT NULL DEFAULT 0,
                cash_200 INTEGER NOT NULL DEFAULT 0,
                cash_500 INTEGER NOT NULL DEFAULT 0,
                cash_1000 INTEGER NOT NULL DEFAULT 0,
                cash_2000 INTEGER NOT NULL DEFAULT 0,

                payee_id INTEGER,
                requisite TEXT,

                FOREIGN KEY(actor_card) REFERENCES cards(card_number),
                FOREIGN KEY(account_id) REFERENCES accounts(account_id),
                FOREIGN KEY(target_account_id) REFERENCES accounts(account_id),
                FOREIGN KEY(subject_card) REFERENCES cards(card_number)
            );
        )";

        char* error_message = nullptr;
        int rc = sqlite3_exec(handle_.get(), sql, nullptr, nullptr, &error_message);

        if (rc != SQLITE_OK) {
            std::string message = error_message ? error_message : "Schema init error";
            if (error_message) sqlite3_free(error_message);
            throw StorageError("init", rc, message);
        }
    }

} // atm::infrastructure::sql

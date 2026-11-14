#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"

#include <sqlite3.h>

namespace atm::infrastructure::sql {

    void SqliteCloser::operator()(sqlite3 *db) const noexcept { if (db) sqlite3_close(db); }

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
        if (broken_) {
            throw StorageError("begin", SQLITE_ABORT, "Database connection is in broken state");
        }

        char* error_message = nullptr;
        int rc = sqlite3_exec(handle_.get(), "BEGIN IMMEDIATE;", nullptr, nullptr, &error_message);

        if (rc != SQLITE_OK) {
            std::string message = error_message ? error_message : "Begin transaction error";
            if(error_message) sqlite3_free(error_message);
            throw StorageError("begin", rc , message);
        }
    }

    void Database::commit()
    {
        if (broken_) {
            throw StorageError("commit", SQLITE_ABORT, "Database connection is in broken state");
        }
        
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

    sqlite3* Database::handle() const {
        if (broken_) {
            throw StorageError("Database::handle", SQLITE_ABORT, "Database connection is in broken state");
        }
        return handle_.get();
    }

    void Database::init()
    {
        const char* sql = R"(
            PRAGMA foreign_keys = ON;

            CREATE TABLE IF NOT EXISTS cards (
                card_number TEXT PRIMARY KEY CHECK (length(card_number) = 16),
                holder_name TEXT NOT NULL CHECK (length(trim(holder_name)) > 0),
                pin_hash TEXT NOT NULL CHECK (length(pin_hash) > 0),
                kind TEXT NOT NULL CHECK (kind IN ('Client', 'Service')),
                status TEXT NOT NULL CHECK (status IN ('Active', 'Retained')),
                failed_attempts INTEGER NOT NULL DEFAULT 0 CHECK (failed_attempts BETWEEN 0 AND 3)
            );

            CREATE TABLE IF NOT EXISTS accounts (
                account_id INTEGER PRIMARY KEY CHECK (account_id > 0),
                card_number TEXT NOT NULL,
                balance_kop INTEGER NOT NULL DEFAULT 0 CHECK (balance_kop >= 0),
                is_primary INTEGER NOT NULL DEFAULT 0 CHECK (is_primary IN (0, 1)),
                FOREIGN KEY(card_number) REFERENCES cards(card_number)
            );

            CREATE UNIQUE INDEX IF NOT EXISTS account_ids_is_primary 
            ON accounts(card_number) WHERE is_primary = 1;

            CREATE TABLE IF NOT EXISTS banknotes (
                denomination INTEGER PRIMARY KEY CHECK (denomination IN (100, 200, 500, 1000, 2000)),
                count INTEGER NOT NULL DEFAULT 0 CHECK (count >= 0)
            );

            CREATE TABLE IF NOT EXISTS journal (
                journal_id INTEGER PRIMARY KEY AUTOINCREMENT,
                at_utc INTEGER NOT NULL,
                local_day TEXT NOT NULL CHECK (length(local_day) = 10),
                kind TEXT NOT NULL CHECK (length(kind) > 0),
                actor_card TEXT NOT NULL,
                account_id INTEGER,
                target_account_id INTEGER,
                subject_card TEXT,
                amount_kop INTEGER NOT NULL DEFAULT 0 CHECK (amount_kop >= 0),

                cash_100 INTEGER NOT NULL DEFAULT 0 CHECK (cash_100 >= 0),
                cash_200 INTEGER NOT NULL DEFAULT 0 CHECK (cash_200 >= 0),
                cash_500 INTEGER NOT NULL DEFAULT 0 CHECK (cash_500 >= 0),
                cash_1000 INTEGER NOT NULL DEFAULT 0 CHECK (cash_1000 >= 0),
                cash_2000 INTEGER NOT NULL DEFAULT 0 CHECK (cash_2000 >= 0),

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

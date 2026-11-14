#include "support/mini_test.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/card_repository.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <string>

namespace {

using atm::domain::Card;
using atm::domain::CardKind;
using atm::domain::CardNumber;
using atm::domain::CardStatus;
using atm::infrastructure::sql::CardRepository;
using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::StorageError;

TEST(DB2CardRepository, FindUnknownCardReturnsEmpty) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        CardRepository repo(db);

        auto unknown_number = CardNumber::parse("9999888877776666");
        auto result = repo.find(unknown_number);

        EXPECT_FALSE(result.has_value());
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(DB2CardRepository, PreservesLeadingZerosAndLengthOnReadAndWrite) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        CardRepository repo(db);

        const std::string number = "0000123456789012";
        const CardNumber card_number = CardNumber::parse(number);
        EXPECT_EQ(card_number.str(), number);

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) "
            "VALUES ('0000123456789012', 'Client', 'hash1', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        auto found = repo.find(card_number);
        EXPECT_TRUE(found.has_value());
        EXPECT_EQ(found->number().str(), number);
        EXPECT_EQ(found->number().str().size(), 16u);
        EXPECT_EQ(found->holderName(), std::string("Client"));
        EXPECT_TRUE(found->kind() == CardKind::Client);
        EXPECT_TRUE(found->status() == CardStatus::Active);
        EXPECT_EQ(found->failedAttempts(), 0);

        found->registerFailedAttempt();
        found->registerFailedAttempt();
        found->registerFailedAttempt();
        repo.save(*found);

        auto reloaded = repo.find(card_number);
        EXPECT_TRUE(reloaded.has_value());
        EXPECT_EQ(reloaded->number().str(), number);
        EXPECT_EQ(reloaded->failedAttempts(), 3);
        EXPECT_TRUE(reloaded->status() == CardStatus::Retained);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(DB2CardRepository, EnforcesDatabaseConstraints) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) "
            "VALUES ('0000123456789012', 'Client', 'hash1', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        // Duplicate
        int dup_rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) "
            "VALUES ('0000123456789012', 'Other Client', 'hash2', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(dup_rc != SQLITE_OK);

        // Account for a non-existent card
        int fk_rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) "
            "VALUES (100, '9999000011112222', 5000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(fk_rc != SQLITE_OK);

        // Invalid kind or status
        int kind_rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) "
            "VALUES ('1111222233334444', 'Admin', 'hash3', 'Admin', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(kind_rc != SQLITE_OK);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(DB2CardRepository, RejectsOutOfRangeNumbersBeforeNarrowing) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        // Value outside the range of int
        sqlite3* db_ptr = nullptr;
        EXPECT_TRUE(sqlite3_open(db_path.c_str(), &db_ptr) == SQLITE_OK);
        const char* create_sql =
            "CREATE TABLE cards ("
            "  card_number TEXT PRIMARY KEY,"
            "  holder_name TEXT NOT NULL,"
            "  pin_hash TEXT NOT NULL,"
            "  kind TEXT NOT NULL,"
            "  status TEXT NOT NULL,"
            "  failed_attempts INTEGER NOT NULL"
            ");";
        EXPECT_TRUE(sqlite3_exec(db_ptr, create_sql, nullptr, nullptr, nullptr) == SQLITE_OK);

        const char* insert_sql =
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) "
            "VALUES ('0000123456789012', 'Olena Koval', 'hash1', 'Client', 'Active', 5000000000);";
        EXPECT_TRUE(sqlite3_exec(db_ptr, insert_sql, nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(db_ptr);

        Database db(db_path);
        CardRepository repo(db);

        bool rejected = false;
        try {
            static_cast<void>(repo.find(CardNumber::parse("0000123456789012")));
        } catch (const StorageError&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

} // namespace
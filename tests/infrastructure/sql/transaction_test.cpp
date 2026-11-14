#include "support/mini_test.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/card_repository.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "domain/transaction.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include <sqlite3.h>

#include <filesystem>
#include <string>

namespace {

using atm::domain::Card;
using atm::domain::CardNumber;
using atm::domain::Transaction;
using atm::infrastructure::sql::CardRepository;
using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::StorageError;

TEST(X2Transaction, CommitPreservesChanges) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards VALUES ('1111222233334444', 'Client', 'hash1', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        {
            Transaction transaction(db);
            CardRepository repo(db);

            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            EXPECT_TRUE(cardOpt.has_value());

            Card card = cardOpt.value();
            card.registerFailedAttempt();
            repo.save(card);

            transaction.commit();
        }

        {
            CardRepository repo(db);
            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            EXPECT_TRUE(cardOpt.has_value());
            EXPECT_EQ(cardOpt.value().failedAttempts(), 1u);
        }
    }

    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);
}

TEST(X2Transaction, RollbackIfCommitNotCalled) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards VALUES ('1111222233334444', 'Client', 'hash1', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        {
            Transaction transaction(db);
            CardRepository repo(db);

            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            EXPECT_TRUE(cardOpt.has_value());
            Card card = cardOpt.value();
            card.registerFailedAttempt();
            repo.save(card);
        }

        {
            CardRepository repo(db);
            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            EXPECT_TRUE(cardOpt.has_value());
            EXPECT_EQ(cardOpt.value().failedAttempts(), 0u);
        }
    }

    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);
}

TEST(X2Transaction, FailedBeginCleansUpUncreatedTransaction) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards VALUES ('1111222233334444', 'Client', 'hash1', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        // First transaction
        Transaction outer_transaction(db);
        CardRepository repo(db);

        auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
        EXPECT_TRUE(cardOpt.has_value());
        Card card = cardOpt.value();
        card.registerFailedAttempt();
        repo.save(card);

        // Second transaction
        bool begin_failed = false;
        try {
            Transaction inner_transaction(db);
        } catch (const StorageError&) {
            begin_failed = true;
        }
        EXPECT_TRUE(begin_failed);

        outer_transaction.commit();

        auto reloaded = repo.find(CardNumber::parse("1111222233334444"));
        EXPECT_TRUE(reloaded.has_value());
        EXPECT_EQ(reloaded->failedAttempts(), 1u);
    }

    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);
}

TEST(X2Transaction, RollbackAfterFailedCommit) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards VALUES ('1111222233334444', 'User One', 'hash1', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        rc = sqlite3_exec(db.handle(), "PRAGMA defer_foreign_keys = ON;", nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        bool commit_failed = false;
        try {
            Transaction transaction(db);
            CardRepository repo(db);

            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            EXPECT_TRUE(cardOpt.has_value());
            Card card = cardOpt.value();
            card.registerFailedAttempt();
            repo.save(card);

            int bad_rc = sqlite3_exec(
                db.handle(),
                "INSERT INTO accounts VALUES (99, '9999888877776666', 1000, 1);",
                nullptr, nullptr, nullptr);
            EXPECT_TRUE(bad_rc == SQLITE_OK);

            transaction.commit();
        } catch (const StorageError&) {
            commit_failed = true;
        }
        EXPECT_TRUE(commit_failed);

        EXPECT_TRUE(sqlite3_get_autocommit(db.handle()) != 0);

        CardRepository repo(db);
        auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
        EXPECT_TRUE(cardOpt.has_value());
        EXPECT_EQ(cardOpt->failedAttempts(), 0u);
    }

    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);
}

TEST(X2Transaction, FailedRollbackMarksDatabaseBroken) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards VALUES ('1111222233334444', 'Client', 'hash1', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        {
            Transaction transaction(db);
            int raw_rc = sqlite3_exec(db.handle(), "ROLLBACK;", nullptr, nullptr, nullptr);
            EXPECT_TRUE(raw_rc == SQLITE_OK);
        }

        bool query_rejected = false;
        try {
            CardRepository repo(db);
            static_cast<void>(repo.find(CardNumber::parse("1111222233334444")));
        } catch (const StorageError&) {
            query_rejected = true;
        }
        EXPECT_TRUE(query_rejected);
    }

    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);
}

} // namespace
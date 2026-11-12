#include "support/mini_test.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/card_repository.hpp"
#include "domain/transaction.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include <sqlite3.h>

#include <filesystem>
#include <string>

namespace {

using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::CardRepository;
using atm::domain::Transaction;
using atm::domain::Card;
using atm::domain::CardNumber;

TEST(X2Transaction, CommitPreservesChanges) {
    std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        {
            char* err = nullptr;
            std::string sql = "INSERT INTO cards VALUES ('1111222233334444', 'User', 'hash1', 'Client', 'Active', 0);";
            sqlite3_exec(db.handle(), sql.c_str(), nullptr, nullptr, &err);
        }

        {
            Transaction tx(db);
            CardRepository repo(db);
            
            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            EXPECT_TRUE(cardOpt.has_value());
            
            Card card = cardOpt.value();
            card.registerFailedAttempt(); // 1
            repo.save(card);

            tx.commit();
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

TEST(X2Transaction, RollbackOIfCommitNotCalled) {
    std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        {
            char* err = nullptr;
            std::string sql = "INSERT INTO cards VALUES ('1111222233334444', 'User One', 'hash1', 'Client', 'Active', 0);";
            sqlite3_exec(db.handle(), sql.c_str(), nullptr, nullptr, &err);
        }

        {
            Transaction tx(db); // db.begin()
            CardRepository repo(db);
            
            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            Card card = cardOpt.value();
            card.registerFailedAttempt(); // changes to 1 in memory
            repo.save(card);

            //no commit
            //~Transaction() -> rollback()
        }

        {
            CardRepository repo(db);
            auto cardOpt = repo.find(CardNumber::parse("1111222233334444"));
            EXPECT_TRUE(cardOpt.has_value());
            EXPECT_EQ(cardOpt.value().failedAttempts(), 0u); //expected 0
        }
    }

    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);
}

} // namespace
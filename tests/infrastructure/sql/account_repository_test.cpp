#include <filesystem>
#include <string>
#include <sqlite3.h>

#include "domain/account.hpp"
#include "domain/card_number.hpp"
#include "domain/money.hpp"
#include "infrastructure/sql/account_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "support/mini_test.hpp"

namespace {

using atm::domain::Account;
using atm::domain::CardNumber;
using atm::domain::Money;
using atm::infrastructure::sql::AccountRepository;
using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::StorageError;

TEST(AccountRepository, FindsListsInOrderAndUpdatesAccounts) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        AccountRepository repo(db);

        const auto alice = CardNumber::parse("4000000000000001");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Alice', 'h1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Bob', 'h2', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(2, '4000000000000001', 250000, 0),"
            "(1, '4000000000000001', 1000000, 1),"
            "(3, '4000000000000002', 5000000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        EXPECT_FALSE(repo.find(999).has_value());
        EXPECT_FALSE(repo.findPrimary(CardNumber::parse("9000000000009999")).has_value());

        const auto primary = repo.findPrimary(alice);
        ASSERT_TRUE(primary.has_value());
        EXPECT_EQ(primary->id(), 1);
        EXPECT_EQ(primary->balance(), Money::uah(10'000));
        EXPECT_TRUE(primary->isPrimary());

        const auto aliceAccounts = repo.listByCard(alice);
        ASSERT_EQ(aliceAccounts.size(), std::size_t{ 2 });
        EXPECT_EQ(aliceAccounts[0].id(), 1);
        EXPECT_EQ(aliceAccounts[1].id(), 2);

        Account updated = *primary;
        updated.debit(Money::uah(1'500));
        repo.save(updated);

        const auto reloaded = repo.find(1);
        ASSERT_TRUE(reloaded.has_value());
        EXPECT_EQ(reloaded->balance(), Money::uah(8'500));
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(AccountRepository, RejectsMissingAccountAndPreservesPrimaryOnConflict) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        AccountRepository repo(db);

        const auto alice = CardNumber::parse("4000000000000001");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Alice', 'h1', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 1000000, 1),"
            "(2, '4000000000000001', 250000, 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        EXPECT_THROW(repo.save(Account(999, alice, Money::uah(500), false)), StorageError);
        EXPECT_FALSE(repo.find(999).has_value());

        // Promoting a second account to primary must fail and keep both existing accounts intact
        EXPECT_THROW(repo.save(Account(2, alice, Money::uah(300), true)), StorageError);

        const auto account1 = repo.find(1);
        const auto account2 = repo.find(2);
        ASSERT_TRUE(account1.has_value());
        ASSERT_TRUE(account2.has_value());
        EXPECT_TRUE(account1->isPrimary());
        EXPECT_EQ(account1->balance(), Money::uah(10'000));
        EXPECT_FALSE(account2->isPrimary());
        EXPECT_EQ(account2->balance(), Money::uah(2'500));
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

} // namespace
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
        AccountRepository repository(db);

        const auto client1 = CardNumber::parse("4000000000000001");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Client1', 'hash1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Client2', 'hash2', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(2, '4000000000000001', 250000, 0),"
            "(1, '4000000000000001', 1000000, 1),"
            "(3, '4000000000000002', 5000000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        EXPECT_FALSE(repository.find(999).has_value());
        EXPECT_FALSE(repository.findPrimary(CardNumber::parse("9000000000009999")).has_value());
        EXPECT_TRUE(repository.listByCard(CardNumber::parse("9000000000009999")).empty());

        const auto primary = repository.findPrimary(client1);
        ASSERT_TRUE(primary.has_value());
        EXPECT_EQ(primary->id(), 1);
        EXPECT_EQ(primary->balance(), Money::uah(10'000));
        EXPECT_TRUE(primary->isPrimary());

        const auto accounts = repository.listByCard(client1);
        ASSERT_EQ(accounts.size(), std::size_t{ 2 });
        EXPECT_EQ(accounts[0].id(), 1);
        EXPECT_EQ(accounts[1].id(), 2);

        Account updated = *primary;
        updated.debit(Money::uah(1'500));
        repository.save(updated);

        const auto reloaded = repository.find(1);
        ASSERT_TRUE(reloaded.has_value());
        EXPECT_EQ(reloaded->balance(), Money::uah(8'500));
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(AccountRepository, RejectsInvalidSavesWithoutModifyingAccounts) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        AccountRepository repo(db);

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Client', 'hash1', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 1000000, 1),"
            "(2, '4000000000000001', 250000, 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        EXPECT_THROW(repo.save(Account(999, CardNumber::parse("4000000000000001"), Money::uah(500), false)), StorageError);
        EXPECT_FALSE(repo.find(999).has_value());
        // Reject setting a second primary account for the same card and leave existing records unchanged.
        EXPECT_THROW(repo.save(Account(2, CardNumber::parse("4000000000000001"), Money::uah(300), true)), StorageError);

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
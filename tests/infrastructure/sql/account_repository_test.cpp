#include <filesystem>
#include <string>
#include <sqlite3.h>

#include "domain/account.hpp"
#include "domain/card_number.hpp"
#include "domain/money.hpp"
#include "infrastructure/sql/account_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "support/mini_test.hpp"

namespace {

using atm::domain::Account;
using atm::domain::CardNumber;
using atm::domain::Money;
using atm::infrastructure::sql::AccountRepository;
using atm::infrastructure::sql::Database;

TEST(AccountRepository, SavesFindsAndUpdatesAccounts) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        AccountRepository repo(db);

        const auto alice = CardNumber::parse("4000000000000001");
        const auto bob = CardNumber::parse("4000000000000002");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Alice', 'h1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Bob', 'h2', 'Client', 'Active', 0);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        repo.save(Account(1, alice, Money::uah(10'000), true));
        repo.save(Account(2, alice, Money::uah(2'500), false));
        repo.save(Account(3, bob, Money::uah(50'000), true));

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

} // namespace
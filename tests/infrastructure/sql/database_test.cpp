#include "support/mini_test.hpp"
#include "domain/transaction.hpp"
#include "infrastructure/sql/data_seeder.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <fstream>

namespace {

using atm::domain::Account;
using atm::domain::BanknoteSet;
using atm::domain::Card;
using atm::domain::CardKind;
using atm::domain::CardNumber;
using atm::domain::CardStatus;
using atm::domain::Denomination;
using atm::domain::EntryKind;
using atm::domain::Money;
using atm::domain::Transaction;
using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::StorageError;
using atm::infrastructure::sql::seedDemoDataIfEmpty;
using atm::infrastructure::sql::validateDatabase;

TEST(DB1Database, DBInitialAndRepeatedLaunch) {
    const std::string db_path = "test_db.db";

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    const CardNumber client_number = CardNumber::parse("4000000000000001");
    const CardNumber service_number = CardNumber::parse("9000000000000001");

    const std::vector<Card> initial_cards = {
        Card(client_number, "Client", "hash1", CardKind::Client, CardStatus::Active, 0),
        Card(service_number, "Service Operator", "hash2", CardKind::Service, CardStatus::Active, 0)
    };

    const std::vector<Account> initial_accounts = {
        Account(1, client_number, Money::uah(30000), true),
        Account(2, client_number, Money::uah(5000), false)
    };

    //Initial Launch
    {
        Database db(db_path);
        EXPECT_TRUE(db.handle() != nullptr);

        seedDemoDataIfEmpty(db, initial_cards, initial_accounts);
        validateDatabase(db);

        EXPECT_TRUE(db.cards().find(client_number).has_value());
        EXPECT_TRUE(db.cards().find(service_number).has_value());
        EXPECT_TRUE(db.accounts().listByCard(client_number).size() == 2);
        EXPECT_TRUE(db.cash().load() == BanknoteSet{});
        EXPECT_TRUE(db.journal().last(10).empty());

        {
            Transaction transaction(db);

            auto account = db.accounts().find(1);
            EXPECT_TRUE(account.has_value());
            account->debit(Money::uah(2500)); // 30000 -> 27500 uah
            db.accounts().save(*account);

            BanknoteSet updated_notes = db.cash().load().with(Denomination::UAH500, 12);
            db.cash().save(updated_notes);

            auto card = db.cards().find(client_number);
            EXPECT_TRUE(card.has_value());
            card->registerFailedAttempt();
            card->registerFailedAttempt();
            card->registerFailedAttempt(); // Retained
            db.cards().save(*card);

            int rc = sqlite3_exec(
                db.handle(),
                "INSERT INTO journal (at_utc, local_day, kind, actor_card, amount_kop) "
                "VALUES (1700000000, '2026-11-14', 'CardRetained', '4000000000000001', 0);",
                nullptr, nullptr, nullptr);
            EXPECT_TRUE(rc == SQLITE_OK);

            transaction.commit();
        }
    }

    EXPECT_TRUE(std::filesystem::exists(db_path));

    // Repeated Launch
    {
        Database db(db_path);
        EXPECT_TRUE(db.handle() != nullptr);

        seedDemoDataIfEmpty(db, initial_cards, initial_accounts);
        validateDatabase(db);

        auto account1 = db.accounts().find(1);
        auto account2 = db.accounts().find(2);
        EXPECT_TRUE(account1.has_value() && account1->balance() == Money::uah(27500));
        EXPECT_TRUE(account2.has_value() && account2->balance() == Money::uah(5000));

        EXPECT_TRUE(db.cash().load() == BanknoteSet{}.with(Denomination::UAH500, 12));

        auto card = db.cards().find(client_number);
        EXPECT_TRUE(card.has_value());
        EXPECT_TRUE(card->failedAttempts() == 3);
        EXPECT_TRUE(card->status() == CardStatus::Retained);

        auto entries = db.journal().last(10);
        EXPECT_TRUE(entries.size() == 1);
        EXPECT_TRUE(entries[0].kind == EntryKind::CardRetained);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(DB1Database, RejectsIncompleteOrIncompatibleDatabase) {
    const std::string db_path = "test_db.db";

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    const CardNumber client_number = CardNumber::parse("4000000000000001");
    const std::vector<Card> initial_cards = { Card(client_number, "Client", "hash1", CardKind::Client, CardStatus::Active, 0)};
    const std::vector<Account> initial_accounts = {Account(1, client_number, Money::uah(10000), true)};

    // Incomplete table banknotes (deleted denomination 200)
    {
        Database db(db_path);
        seedDemoDataIfEmpty(db, initial_cards, initial_accounts);

        int rc = sqlite3_exec(db.handle(), "DELETE FROM banknotes WHERE denomination = 200;", nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        bool rejected = false;
        try {
            validateDatabase(db);
        } catch (const StorageError&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    }

    std::filesystem::remove(db_path);

    // Client Card without primary account
    {
        Database db(db_path);
        seedDemoDataIfEmpty(db, initial_cards, initial_accounts);

        int rc = sqlite3_exec(db.handle(), "UPDATE accounts SET is_primary = 0;", nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        bool rejected = false;
        try {
            validateDatabase(db);
        } catch (const StorageError&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(X3Database, InvalidDatabasePath) {
    const std::string invalid_path = "fake_folder/subfolder/database.db";

    bool exception_thrown = false;
    try {
        Database db(invalid_path);
    } catch (const StorageError&) {
        exception_thrown = true;
    }

    EXPECT_TRUE(exception_thrown);
}

TEST(X3Database, FailedInitClosesConnectionAndReleasesFile) {
    const std::string corrupt_path = "test_db.db";
    if (std::filesystem::exists(corrupt_path)) {
        std::filesystem::remove(corrupt_path);
    }

    {
        std::ofstream out(corrupt_path, std::ios::binary);
        out << "Not a valid sqlite database header";
    }

    bool exception = false;
    try {
        Database db(corrupt_path);
    } catch (const StorageError&) {
        exception = true;
    }
    EXPECT_TRUE(exception);

    std::error_code ec;
    EXPECT_TRUE(std::filesystem::remove(corrupt_path, ec));
    EXPECT_TRUE(!ec);
}

} // namespace
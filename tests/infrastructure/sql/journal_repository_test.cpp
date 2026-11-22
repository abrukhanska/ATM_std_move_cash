#include <filesystem>
#include <string>
#include <sqlite3.h>

#include "domain/banknote_set.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/journal_entry.hpp"
#include "domain/money.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/journal_repository.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "support/mini_test.hpp"

namespace {

using atm::domain::BanknoteSet;
using atm::domain::CardNumber;
using atm::domain::Denomination;
using atm::domain::EntryKind;
using atm::domain::JournalEntry;
using atm::domain::Money;
using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::JournalRepository;
using atm::infrastructure::sql::StorageError;

TEST(JournalRepository, AppendsAndReadsLastEntriesInReverseOrder) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        JournalRepository repo(db);

        const auto alice = CardNumber::parse("4000000000000001");
        const auto bob = CardNumber::parse("4000000000000002");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Alice', 'h1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Bob', 'h2', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 500000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        JournalEntry e1;
        e1.atUtc = 1'768'471'100;
        e1.localDay = "2026-01-15";
        e1.kind = EntryKind::CardReleased;
        e1.actor = alice;
        e1.subjectCard = bob;

        JournalEntry e2;
        e2.atUtc = 1'768'471'200;
        e2.localDay = "2026-01-15";
        e2.kind = EntryKind::PinChange;
        e2.actor = alice;

        JournalEntry e3;
        e3.atUtc = 1'768'471'300;
        e3.localDay = "2026-01-15";
        e3.kind = EntryKind::Withdrawal;
        e3.actor = alice;
        e3.account = 1;
        e3.amount = Money::uah(500);
        e3.cash = BanknoteSet{}.with(Denomination::UAH500, 1);

        const JournalEntry saved1 = repo.append(e1);
        const JournalEntry saved2 = repo.append(e2);
        const JournalEntry saved3 = repo.append(e3);

        EXPECT_TRUE(saved1.id > 0);
        EXPECT_TRUE(saved2.id > saved1.id);
        EXPECT_TRUE(saved3.id > saved2.id);

        EXPECT_TRUE(repo.last(0).empty());

        const auto latestTwo = repo.last(2);
        ASSERT_EQ(latestTwo.size(), std::size_t{ 2 });
        EXPECT_EQ(latestTwo[0], saved3);
        EXPECT_EQ(latestTwo[1], saved2);

        const auto all = repo.last(10);
        ASSERT_EQ(all.size(), std::size_t{ 3 });
        EXPECT_EQ(all[0], saved3);
        EXPECT_EQ(all[1], saved2);
        EXPECT_EQ(all[2], saved1);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(JournalRepository, CalculatesWithdrawnOnAndSumAmount) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        JournalRepository repo(db);

        const auto alice = CardNumber::parse("4000000000000001");
        const auto bob = CardNumber::parse("4000000000000002");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Alice', 'h1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Bob', 'h2', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 1000000, 1),"
            "(2, '4000000000000002', 1000000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        auto makeWithdrawal = [](const CardNumber& actor, int accountId, const std::string& day, BanknoteSet notes) {
            JournalEntry entry;
            entry.atUtc = 1'768'471'000;
            entry.localDay = day;
            entry.kind = EntryKind::Withdrawal;
            entry.actor = actor;
            entry.account = accountId;
            entry.amount = notes.total();
            entry.cash = notes;
            return entry;
        };

        (void)repo.append(makeWithdrawal(alice, 1, "2026-01-15", BanknoteSet{}.with(Denomination::UAH500, 2)));
        (void)repo.append(makeWithdrawal(alice, 1, "2026-01-15", BanknoteSet{}.with(Denomination::UAH500, 1)));
        (void)repo.append(makeWithdrawal(alice, 1, "2026-01-16", BanknoteSet{}.with(Denomination::UAH1000, 2)));
        (void)repo.append(makeWithdrawal(bob, 2, "2026-01-15", BanknoteSet{}.with(Denomination::UAH1000, 3)));

        JournalEntry pinChange;
        pinChange.atUtc = 1'768'471'500;
        pinChange.localDay = "2026-01-15";
        pinChange.kind = EntryKind::PinChange;
        pinChange.actor = alice;
        (void)repo.append(pinChange);

        EXPECT_EQ(repo.withdrawnOn(alice, "2026-01-15"), Money::uah(1'500));
        EXPECT_EQ(repo.withdrawnOn(alice, "2026-01-16"), Money::uah(2'000));
        EXPECT_EQ(repo.withdrawnOn(bob, "2026-01-15"), Money::uah(3'000));
        EXPECT_EQ(repo.withdrawnOn(alice, "2026-01-20"), Money{});

        EXPECT_EQ(repo.sumAmount(EntryKind::Withdrawal), Money::uah(6'500));
        EXPECT_EQ(repo.sumAmount(EntryKind::PinChange), Money{});
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(JournalRepository, RejectsInconsistentEntriesWithoutModifyingJournal) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        JournalRepository repo(db);

        const auto alice = CardNumber::parse("4000000000000001");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Alice', 'h1', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 500000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        // Withdrawal with empty banknotes
        JournalEntry badWithdrawalCash;
        badWithdrawalCash.atUtc = 1'768'471'000;
        badWithdrawalCash.localDay = "2026-01-15";
        badWithdrawalCash.kind = EntryKind::Withdrawal;
        badWithdrawalCash.actor = alice;
        badWithdrawalCash.account = 1;
        badWithdrawalCash.amount = Money::uah(100);
        EXPECT_THROW(repo.append(badWithdrawalCash), StorageError);

        // Withdrawal without account or with payeeId/requisite
        JournalEntry badWithdrawalFields;
        badWithdrawalFields.atUtc = 1'768'471'000;
        badWithdrawalFields.localDay = "2026-01-15";
        badWithdrawalFields.kind = EntryKind::Withdrawal;
        badWithdrawalFields.actor = alice;
        badWithdrawalFields.amount = Money::uah(100);
        badWithdrawalFields.cash = BanknoteSet{}.with(Denomination::UAH100, 1);
        badWithdrawalFields.payeeId = 1;
        badWithdrawalFields.requisite = "0501234567";
        EXPECT_THROW(repo.append(badWithdrawalFields), StorageError);

        // Payment with negative payeeId must be rejected, not coerced to NULL
        JournalEntry badPayment;
        badPayment.atUtc = 1'768'471'000;
        badPayment.localDay = "2026-01-15";
        badPayment.kind = EntryKind::Payment;
        badPayment.actor = alice;
        badPayment.account = 1;
        badPayment.amount = Money::uah(100);
        badPayment.payeeId = -1;
        badPayment.requisite = "0501234567";
        EXPECT_THROW(repo.append(badPayment), StorageError);

        EXPECT_TRUE(repo.last(10).empty());
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

} // namespace
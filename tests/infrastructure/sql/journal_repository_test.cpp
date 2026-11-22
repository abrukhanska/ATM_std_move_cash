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
        JournalRepository repository(db);

        const auto client1 = CardNumber::parse("4000000000000001");
        const auto client2 = CardNumber::parse("4000000000000002");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Client1', 'hash1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Client2', 'hash2', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 500000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        JournalEntry e1;
        e1.atUtc = 1'768'471'100;
        e1.localDay = "2026-01-15";
        e1.kind = EntryKind::CardReleased;
        e1.actor = client1;
        e1.subjectCard = client2;

        JournalEntry e2;
        e2.atUtc = 1'768'471'200;
        e2.localDay = "2026-01-15";
        e2.kind = EntryKind::PinChange;
        e2.actor = client1;

        JournalEntry e3;
        e3.atUtc = 1'768'471'300;
        e3.localDay = "2026-01-15";
        e3.kind = EntryKind::Withdrawal;
        e3.actor = client1;
        e3.account = 1;
        e3.amount = Money::uah(500);
        e3.cash = BanknoteSet{}.with(Denomination::UAH500, 1);

        const JournalEntry saved1 = repository.append(e1);
        const JournalEntry saved2 = repository.append(e2);
        const JournalEntry saved3 = repository.append(e3);

        EXPECT_TRUE(saved1.id > 0);
        EXPECT_TRUE(saved2.id > saved1.id);
        EXPECT_TRUE(saved3.id > saved2.id);

        EXPECT_TRUE(repository.last(0).empty());

        const auto latest2 = repository.last(2);
        ASSERT_EQ(latest2.size(), std::size_t{ 2 });
        EXPECT_EQ(latest2[0], saved3);
        EXPECT_EQ(latest2[1], saved2);

        const auto all = repository.last(10);
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
        JournalRepository repository(db);

        const auto client1 = CardNumber::parse("4000000000000001");
        const auto client2 = CardNumber::parse("4000000000000002");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Client1', 'hash1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Client2', 'hash2', 'Client', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 1000000, 1),"
            "(2, '4000000000000002', 1000000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        auto makeWithdrawal = [](const CardNumber& actor, int accountId, const std::string& day, BanknoteSet banknotes) {
            JournalEntry entry;
            entry.atUtc = 1'768'471'000;
            entry.localDay = day;
            entry.kind = EntryKind::Withdrawal;
            entry.actor = actor;
            entry.account = accountId;
            entry.amount = banknotes.total();
            entry.cash = banknotes;
            return entry;
        };

        (void)repository.append(makeWithdrawal(client1, 1, "2026-01-15", BanknoteSet{}.with(Denomination::UAH500, 2)));
        (void)repository.append(makeWithdrawal(client1, 1, "2026-01-15", BanknoteSet{}.with(Denomination::UAH500, 1)));
        (void)repository.append(makeWithdrawal(client1, 1, "2026-01-16", BanknoteSet{}.with(Denomination::UAH1000, 2)));
        (void)repository.append(makeWithdrawal(client2, 2, "2026-01-15", BanknoteSet{}.with(Denomination::UAH1000, 3)));

        JournalEntry entry;
        entry.atUtc = 1'768'471'500;
        entry.localDay = "2026-01-15";
        entry.kind = EntryKind::PinChange;
        entry.actor = client1;
        (void)repository.append(entry);

        EXPECT_EQ(repository.withdrawnOn(client1, "2026-01-15"), Money::uah(1'500));
        EXPECT_EQ(repository.withdrawnOn(client1, "2026-01-16"), Money::uah(2'000));
        EXPECT_EQ(repository.withdrawnOn(client2, "2026-01-15"), Money::uah(3'000));
        EXPECT_EQ(repository.withdrawnOn(client1, "2026-01-20"), Money{});

        EXPECT_EQ(repository.sumAmount(EntryKind::Withdrawal), Money::uah(6'500));
        EXPECT_EQ(repository.sumAmount(EntryKind::PinChange), Money{});
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(JournalRepository, RejectsInconsistentEntries) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        JournalRepository repository(db);

        const auto client1 = CardNumber::parse("4000000000000001");
        const auto client2 = CardNumber::parse("4000000000000002");
        const auto service = CardNumber::parse("4000000000000099");

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES "
            "('4000000000000001', 'Client1', 'hash1', 'Client', 'Active', 0),"
            "('4000000000000002', 'Client2', 'hash2', 'Client', 'Active', 0),"
            "('4000000000000099', 'Operator', 'hash3', 'Service', 'Active', 0);"
            "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES "
            "(1, '4000000000000001', 1000000, 1),"
            "(2, '4000000000000002', 1000000, 1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        // negative scenarios

        {
            // Withdrawal: missing account
            JournalEntry withdrawal1;
            withdrawal1.atUtc = 1'768'471'000;
            withdrawal1.localDay = "2026-01-15";
            withdrawal1.kind = EntryKind::Withdrawal;
            withdrawal1.actor = client1;
            withdrawal1.amount = Money::uah(100);
            withdrawal1.cash = BanknoteSet{}.with(Denomination::UAH100, 1);
            EXPECT_THROW((void)repository.append(withdrawal1), StorageError);

            // Withdrawal: zero amount
            JournalEntry withdrawal2;
            withdrawal2.atUtc = 1'768'471'000;
            withdrawal2.localDay = "2026-01-15";
            withdrawal2.kind = EntryKind::Withdrawal;
            withdrawal2.actor = client1;
            withdrawal2.account = 1;
            withdrawal2.amount = Money{};
            EXPECT_THROW((void)repository.append(withdrawal2), StorageError);

            // Withdrawal: cash total != amount
            JournalEntry withdrawal3;
            withdrawal3.atUtc = 1'768'471'000;
            withdrawal3.localDay = "2026-01-15";
            withdrawal3.kind = EntryKind::Withdrawal;
            withdrawal3.actor = client1;
            withdrawal3.account = 1;
            withdrawal3.amount = Money::uah(200);
            withdrawal3.cash = BanknoteSet{}.with(Denomination::UAH100, 1);
            EXPECT_THROW((void)repository.append(withdrawal3), StorageError);

            // Withdrawal: unexpected payeeId and requisite
            JournalEntry withdrawal4;
            withdrawal4.atUtc = 1'768'471'000;
            withdrawal4.localDay = "2026-01-15";
            withdrawal4.kind = EntryKind::Withdrawal;
            withdrawal4.actor = client1;
            withdrawal4.account = 1;
            withdrawal4.amount = Money::uah(100);
            withdrawal4.cash = BanknoteSet{}.with(Denomination::UAH100, 1);
            withdrawal4.payeeId = 1;
            withdrawal4.requisite = "+380501234567";
            EXPECT_THROW((void)repository.append(withdrawal4), StorageError);

            // Deposit: missing account
            JournalEntry deposit1;
            deposit1.atUtc = 1'768'471'000;
            deposit1.localDay = "2026-01-15";
            deposit1.kind = EntryKind::Deposit;
            deposit1.actor = client1;
            deposit1.amount = Money::uah(200);
            deposit1.cash = BanknoteSet{}.with(Denomination::UAH200, 1);
            EXPECT_THROW((void)repository.append(deposit1), StorageError);

            // Deposit: cash total != amount
            JournalEntry deposit2;
            deposit2.atUtc = 1'768'471'000;
            deposit2.localDay = "2026-01-15";
            deposit2.kind = EntryKind::Deposit;
            deposit2.actor = client1;
            deposit2.account = 1;
            deposit2.amount = Money::uah(500);
            deposit2.cash = BanknoteSet{}.with(Denomination::UAH200, 1);
            EXPECT_THROW((void)repository.append(deposit2), StorageError);

            // Transfer: missing target account
            JournalEntry transfer1;
            transfer1.atUtc = 1'768'471'000;
            transfer1.localDay = "2026-01-15";
            transfer1.kind = EntryKind::Transfer;
            transfer1.actor = client1;
            transfer1.account = 1;
            transfer1.amount = Money::uah(300);
            EXPECT_THROW((void)repository.append(transfer1), StorageError);

            // Transfer: target == source account
            JournalEntry transfer2;
            transfer2.atUtc = 1'768'471'000;
            transfer2.localDay = "2026-01-15";
            transfer2.kind = EntryKind::Transfer;
            transfer2.actor = client1;
            transfer2.account = 1;
            transfer2.target = 1;
            transfer2.amount = Money::uah(300);
            EXPECT_THROW((void)repository.append(transfer2), StorageError);

            // Transfer: unexpected banknotes
            JournalEntry transfer3;
            transfer3.atUtc = 1'768'471'000;
            transfer3.localDay = "2026-01-15";
            transfer3.kind = EntryKind::Transfer;
            transfer3.actor = client1;
            transfer3.account = 1;
            transfer3.target = 2;
            transfer3.amount = Money::uah(300);
            transfer3.cash = BanknoteSet{}.with(Denomination::UAH100, 3);
            EXPECT_THROW((void)repository.append(transfer3), StorageError);

            // Payment: missing payeeId
            JournalEntry payment1;
            payment1.atUtc = 1'768'471'000;
            payment1.localDay = "2026-01-15";
            payment1.kind = EntryKind::Payment;
            payment1.actor = client1;
            payment1.account = 1;
            payment1.amount = Money::uah(150);
            payment1.requisite = "+380501234567";
            EXPECT_THROW((void)repository.append(payment1), StorageError);

            // Payment: negative payeeId
            JournalEntry payment2;
            payment2.atUtc = 1'768'471'000;
            payment2.localDay = "2026-01-15";
            payment2.kind = EntryKind::Payment;
            payment2.actor = client1;
            payment2.account = 1;
            payment2.amount = Money::uah(150);
            payment2.payeeId = -1;
            payment2.requisite = "+380501234567";
            EXPECT_THROW((void)repository.append(payment2), StorageError);

            // Payment: unexpected banknotes
            JournalEntry payment3;
            payment3.atUtc = 1'768'471'000;
            payment3.localDay = "2026-01-15";
            payment3.kind = EntryKind::Payment;
            payment3.actor = client1;
            payment3.account = 1;
            payment3.amount = Money::uah(150);
            payment3.payeeId = 1;
            payment3.requisite = "+380501234567";
            payment3.cash = BanknoteSet{}.with(Denomination::UAH100, 1);
            EXPECT_THROW((void)repository.append(payment3), StorageError);

            // PinChange: unexpected amount
            JournalEntry pinChange1;
            pinChange1.atUtc = 1'768'471'000;
            pinChange1.localDay = "2026-01-15";
            pinChange1.kind = EntryKind::PinChange;
            pinChange1.actor = client1;
            pinChange1.amount = Money::uah(50);
            EXPECT_THROW((void)repository.append(pinChange1), StorageError);

            // CardRetained: unexpected account
            JournalEntry cardRetained1;
            cardRetained1.atUtc = 1'768'471'000;
            cardRetained1.localDay = "2026-01-15";
            cardRetained1.kind = EntryKind::CardRetained;
            cardRetained1.actor = client1;
            cardRetained1.subjectCard = client1;
            cardRetained1.account = 1;
            EXPECT_THROW((void)repository.append(cardRetained1), StorageError);

            // CardReleased: missing subjectCard
            JournalEntry cardReleased1;
            cardReleased1.atUtc = 1'768'471'000;
            cardReleased1.localDay = "2026-01-15";
            cardReleased1.kind = EntryKind::CardReleased;
            cardReleased1.actor = service;
            EXPECT_THROW((void)repository.append(cardReleased1), StorageError);

            // CardReleased: unexpected amount
            JournalEntry cardReleased2;
            cardReleased2.atUtc = 1'768'471'000;
            cardReleased2.localDay = "2026-01-15";
            cardReleased2.kind = EntryKind::CardReleased;
            cardReleased2.actor = service;
            cardReleased2.subjectCard = client2;
            cardReleased2.amount = Money::uah(100);
            EXPECT_THROW((void)repository.append(cardReleased2), StorageError);

            // CashLoaded: cash total != amount
            JournalEntry cashLoaded1;
            cashLoaded1.atUtc = 1'768'471'000;
            cashLoaded1.localDay = "2026-01-15";
            cashLoaded1.kind = EntryKind::CashLoaded;
            cashLoaded1.actor = service;
            cashLoaded1.cash = BanknoteSet{}.with(Denomination::UAH500, 1);
            cashLoaded1.amount = Money::uah(1000);
            EXPECT_THROW((void)repository.append(cashLoaded1), StorageError);

            // CashLoaded: unexpected account
            JournalEntry cashLoaded2;
            cashLoaded2.atUtc = 1'768'471'000;
            cashLoaded2.localDay = "2026-01-15";
            cashLoaded2.kind = EntryKind::CashLoaded;
            cashLoaded2.actor = service;
            cashLoaded2.account = 1;
            cashLoaded2.cash = BanknoteSet{}.with(Denomination::UAH500, 2);
            cashLoaded2.amount = Money::uah(1000);
            EXPECT_THROW((void)repository.append(cashLoaded2), StorageError);

            // CashUnloaded: empty cash and zero amount
            JournalEntry cashUnloaded1;
            cashUnloaded1.atUtc = 1'768'471'000;
            cashUnloaded1.localDay = "2026-01-15";
            cashUnloaded1.kind = EntryKind::CashUnloaded;
            cashUnloaded1.actor = service;
            EXPECT_THROW((void)repository.append(cashUnloaded1), StorageError);

            // DeviceOutOfService: unexpected account
            JournalEntry deviceOutOfService1;
            deviceOutOfService1.atUtc = 1'768'471'000;
            deviceOutOfService1.localDay = "2026-01-15";
            deviceOutOfService1.kind = EntryKind::DeviceOutOfService;
            deviceOutOfService1.actor = service;
            deviceOutOfService1.account = 1;
            EXPECT_THROW((void)repository.append(deviceOutOfService1), StorageError);

            // DeviceInService: unexpected amount
            JournalEntry deviceInService1;
            deviceInService1.atUtc = 1'768'471'000;
            deviceInService1.localDay = "2026-01-15";
            deviceInService1.kind = EntryKind::DeviceInService;
            deviceInService1.actor = service;
            deviceInService1.amount = Money::uah(100);
            EXPECT_THROW((void)repository.append(deviceInService1), StorageError);

            EXPECT_TRUE(repository.last(10).empty());
        }

        // positive scenarios

        {
            JournalEntry withdrawal;
            withdrawal.atUtc = 1'768'471'000;
            withdrawal.localDay = "2026-01-15";
            withdrawal.kind = EntryKind::Withdrawal;
            withdrawal.actor = client1;
            withdrawal.account = 1;
            withdrawal.amount = Money::uah(100);
            withdrawal.cash = BanknoteSet{}.with(Denomination::UAH100, 1);
            const JournalEntry saved1 = repository.append(withdrawal);
            EXPECT_TRUE(saved1.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved1);

            JournalEntry deposit;
            deposit.atUtc = 1'768'471'100;
            deposit.localDay = "2026-01-15";
            deposit.kind = EntryKind::Deposit;
            deposit.actor = client1;
            deposit.account = 1;
            deposit.amount = Money::uah(200);
            deposit.cash = BanknoteSet{}.with(Denomination::UAH200, 1);
            const JournalEntry saved2 = repository.append(deposit);
            EXPECT_TRUE(saved2.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved2);

            JournalEntry transfer;
            transfer.atUtc = 1'768'471'200;
            transfer.localDay = "2026-01-15";
            transfer.kind = EntryKind::Transfer;
            transfer.actor = client1;
            transfer.account = 1;
            transfer.target = 2;
            transfer.amount = Money::uah(300);
            const JournalEntry saved3 = repository.append(transfer);
            EXPECT_TRUE(saved3.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved3);

            JournalEntry payment;
            payment.atUtc = 1'768'471'300;
            payment.localDay = "2026-01-15";
            payment.kind = EntryKind::Payment;
            payment.actor = client1;
            payment.account = 1;
            payment.amount = Money::uah(150);
            payment.payeeId = 1;
            payment.requisite = "+380501234567";
            const JournalEntry saved4 = repository.append(payment);
            EXPECT_TRUE(saved4.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved4);

            JournalEntry pinChange;
            pinChange.atUtc = 1'768'471'400;
            pinChange.localDay = "2026-01-15";
            pinChange.kind = EntryKind::PinChange;
            pinChange.actor = client1;
            const JournalEntry saved5 = repository.append(pinChange);
            EXPECT_TRUE(saved5.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved5);

            JournalEntry cardRetained;
            cardRetained.atUtc = 1'768'471'500;
            cardRetained.localDay = "2026-01-15";
            cardRetained.kind = EntryKind::CardRetained;
            cardRetained.actor = client1;
            cardRetained.subjectCard = client1;
            const JournalEntry saved6 = repository.append(cardRetained);
            EXPECT_TRUE(saved6.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved6);

            JournalEntry cardReleased;
            cardReleased.atUtc = 1'768'471'600;
            cardReleased.localDay = "2026-01-15";
            cardReleased.kind = EntryKind::CardReleased;
            cardReleased.actor = service;
            cardReleased.subjectCard = client2;
            const JournalEntry saved7 = repository.append(cardReleased);
            EXPECT_TRUE(saved7.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved7);

            JournalEntry cashLoaded;
            cashLoaded.atUtc = 1'768'471'700;
            cashLoaded.localDay = "2026-01-15";
            cashLoaded.kind = EntryKind::CashLoaded;
            cashLoaded.actor = service;
            cashLoaded.cash = BanknoteSet{}.with(Denomination::UAH500, 2);
            cashLoaded.amount = Money::uah(1000);
            const JournalEntry saved8 = repository.append(cashLoaded);
            EXPECT_TRUE(saved8.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved8);

            JournalEntry cashUnloaded;
            cashUnloaded.atUtc = 1'768'471'800;
            cashUnloaded.localDay = "2026-01-15";
            cashUnloaded.kind = EntryKind::CashUnloaded;
            cashUnloaded.actor = service;
            cashUnloaded.cash = BanknoteSet{}.with(Denomination::UAH500, 1);
            cashUnloaded.amount = Money::uah(500);
            const JournalEntry saved9 = repository.append(cashUnloaded);
            EXPECT_TRUE(saved9.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved9);

            JournalEntry deviceOutOfService;
            deviceOutOfService.atUtc = 1'768'471'900;
            deviceOutOfService.localDay = "2026-01-15";
            deviceOutOfService.kind = EntryKind::DeviceOutOfService;
            deviceOutOfService.actor = service;
            const JournalEntry saved10 = repository.append(deviceOutOfService);
            EXPECT_TRUE(saved10.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved10);

            JournalEntry deviceInService;
            deviceInService.atUtc = 1'768'472'000;
            deviceInService.localDay = "2026-01-15";
            deviceInService.kind = EntryKind::DeviceInService;
            deviceInService.actor = service;
            const JournalEntry saved11 = repository.append(deviceInService);
            EXPECT_TRUE(saved11.id > 0);
            EXPECT_EQ(repository.last(1)[0], saved11);
        }
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

} // namespace
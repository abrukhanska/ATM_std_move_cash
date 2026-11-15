#include <ctime>
#include <limits>
#include <string_view>
#include "support/financial_sql_fixture.hpp"
#include "support/domain_assertions.hpp"

namespace {
    using namespace atm::test_support::financial_sql;

    Money balance(Fixture& f, AccountId id) {
        return f.db.accounts().find(id).value().balance();
    }

    template<class Operation>
    void expectRejected(Fixture& f, ErrorReason reason, Operation operation) {
        const auto before = snapshot(f.db);
        EXPECT_DOMAIN_ERROR((void)operation(), reason);
        EXPECT_EQ(snapshot(f.db), before);
        EXPECT_FALSE(f.db.broken());
        EXPECT_TRUE(sqlite3_get_autocommit(f.db.handle()) != 0);
    }

    void setBalance(Fixture& f, AccountId id, Money amount) {
        auto account = f.db.accounts().find(id).value();
        Transaction tx(f.db);
        f.db.accounts().save(Account(id, account.owner(), amount, account.isPrimary()));
        tx.commit();
    }

    void setCash(Fixture& f, const BanknoteSet& cash) {
        Transaction tx(f.db);
        f.db.cash().save(cash);
        tx.commit();
    }

    void expectJournal(Fixture& f, const JournalEntry& entry, std::size_t count) {
        const auto journal = f.db.journal().last(count + 1);
        ASSERT_EQ(journal.size(), count);
        EXPECT_EQ(journal.front(), entry);
        EXPECT_TRUE(entry.id > 0);
        EXPECT_EQ(entry.atUtc, toEpochSeconds(f.now));
        EXPECT_EQ(entry.localDay, localDay(f.now));
    }

    TimePoint localTime(int hour, int minute, int second) {
        std::tm value{};
        value.tm_year = 2026 - 1900;
        value.tm_mon = 0;
        value.tm_mday = 15;
        value.tm_hour = hour;
        value.tm_min = minute;
        value.tm_sec = second;
        value.tm_isdst = -1;
        const auto time = std::mktime(&value);
        if (time == static_cast<std::time_t>(-1)) throw std::runtime_error("Invalid test date");
        return std::chrono::system_clock::from_time_t(time);
    }

    TEST(FinancialSql, M2ForeignAccountIsRejectedByEveryClientOperation) {
        Fixture f;
        WithdrawalService withdrawal(f.db, f.config, f.clock);
        DepositService deposit(f.db, f.clock);
        TransferService transfer(f.db, f.clock);
        PaymentService payment(f.db, f.clock);
        const auto preview = transfer.preview(f.client, 1, f.recipient, Money::uah(100));
        expectRejected(f, ErrorReason::AccessDenied, [&] { return withdrawal.withdraw(f.client, 3, Money::uah(100)); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return deposit.deposit(f.client, 3, f.notes); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return transfer.preview(f.client, 3, f.recipient, Money::uah(100)); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return transfer.execute(f.client, 3, preview); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return payment.pay(f.client, 3, 2, "", Money::uah(100)); });
    }

    TEST(FinancialSql, M2ServiceAndRetainedActorsCannotRunClientOperations) {
        for (const bool retained : {false, true}) {
            Fixture f;
            if (retained) sql(f.db, "UPDATE cards SET status='Retained', failed_attempts=3 WHERE card_number='4000000000000001';");
            const auto actor = retained ? f.client : f.service;
            WithdrawalService withdrawal(f.db, f.config, f.clock);
            DepositService deposit(f.db, f.clock);
            PaymentService payment(f.db, f.clock);
            TransferService transfer(f.db, f.clock);
            TransferPreview preview{ f.recipient, 3, "Recipient", Money::uah(100) };
            expectRejected(f, ErrorReason::AccessDenied, [&] { return withdrawal.withdraw(actor, 1, Money::uah(100)); });
            expectRejected(f, ErrorReason::AccessDenied, [&] { return deposit.deposit(actor, 1, f.notes); });
            expectRejected(f, ErrorReason::AccessDenied, [&] { return payment.pay(actor, 1, 2, "", Money::uah(100)); });
            expectRejected(f, ErrorReason::AccessDenied, [&] { return transfer.preview(actor, 1, f.recipient, Money::uah(100)); });
            expectRejected(f, ErrorReason::AccessDenied, [&] { return transfer.execute(actor, 1, preview); });
        }
    }

    TEST(FinancialSql, M2ClientCannotUseCashOrMonitoringServices) {
        Fixture f;
        CashService cash(f.db, f.clock);
        MonitoringService monitor(f.db, f.config);
        expectRejected(f, ErrorReason::AccessDenied, [&] { return cash.inventory(f.client); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return cash.load(f.client, f.notes); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return cash.unload(f.client, f.notes); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return monitor.recent(f.client); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return monitor.totalLoaded(f.client); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return monitor.totalDeposited(f.client); });
        expectRejected(f, ErrorReason::AccessDenied, [&] { return monitor.totalWithdrawn(f.client); });
    }

    TEST(FinancialSql, W1QuickAndCustomWithdrawalsPersistExactValues) {
        Fixture f;
        WithdrawalService service(f.db, f.config, f.clock);
        auto amounts = service.quickAmounts();
        amounts.push_back(Money::uah(600));
        Money total;
        std::size_t count = 0;
        for (auto amount : amounts) {
            const auto entry = service.withdraw(f.client, 1, amount);
            total = total + amount;
            ++count;
            EXPECT_EQ(balance(f, 1), Money::uah(20'000) - total);
            EXPECT_EQ(balance(f, 2), Money::uah(5'000));
            EXPECT_EQ(balance(f, 3), Money::uah(500));
            EXPECT_EQ(f.db.cash().load(), BanknoteSet{}.with(Denomination::UAH100,
                200 - static_cast<std::uint32_t>(total.kopiykas() / 10'000)));
            EXPECT_EQ(entry.kind, EntryKind::Withdrawal);
            EXPECT_EQ(entry.actor, f.client);
            EXPECT_EQ(entry.account, std::optional<AccountId>{1});
            EXPECT_EQ(entry.amount, amount);
            EXPECT_EQ(entry.cash, BanknoteSet{}.with(Denomination::UAH100,
                static_cast<std::uint32_t>(amount.kopiykas() / 10'000)));
            expectJournal(f, entry, count);
        }
        EXPECT_EQ(f.clockCalls, static_cast<int>(count));
    }

    TEST(FinancialSql, W1InvalidAmountsLeaveDatabaseUnchanged) {
        Fixture f;
        WithdrawalService service(f.db, f.config, f.clock);
        for (auto amount : { Money{}, Money::ofKopiykas(1), Money::uah(150) }) {
            expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.withdraw(f.client, 1, amount); });
        }
        EXPECT_EQ(f.clockCalls, 0);
    }

    TEST(FinancialSql, W2BalanceThenLimitThenCash) {
        Fixture f;
        WithdrawalService service(f.db, f.config, f.clock);
        (void)service.withdraw(f.client, 1, Money::uah(9'900));
        setCash(f, BanknoteSet{});
        setBalance(f, 1, Money{});
        expectRejected(f, ErrorReason::InsufficientFunds, [&] { return service.withdraw(f.client, 1, Money::uah(200)); });
        setBalance(f, 1, Money::uah(1'000));
        expectRejected(f, ErrorReason::DailyLimitExceeded, [&] { return service.withdraw(f.client, 1, Money::uah(200)); });
        expectRejected(f, ErrorReason::AtmOutOfCash, [&] { return service.withdraw(f.client, 1, Money::uah(100)); });
    }

    TEST(FinancialSql, W4LimitIsSharedAcrossAccountsButNotCardsOrDays) {
        Fixture f;
        f.now = localTime(12, 0, 0);
        WithdrawalService service(f.db, f.config, f.clock);
        (void)service.withdraw(f.client, 1, Money::uah(9'900));
        (void)service.withdraw(f.client, 2, Money::uah(100));
        expectRejected(f, ErrorReason::DailyLimitExceeded, [&] { return service.withdraw(f.client, 2, Money::uah(100)); });
        EXPECT_EQ(f.db.journal().withdrawnOn(f.client, localDay(f.now)), Money::uah(10'000));
        (void)service.withdraw(f.recipient, 3, Money::uah(100));
        f.now += std::chrono::hours(24);
        EXPECT_EQ(f.db.journal().withdrawnOn(f.client, localDay(f.now)), Money{});
        (void)service.withdraw(f.client, 2, Money::uah(100));
        EXPECT_EQ(f.db.journal().withdrawnOn(f.client, localDay(f.now)), Money::uah(100));
    }

    TEST(FinancialSql, W4OneClockReadAcrossMidnight) {
        Fixture f;
        const auto beforeMidnight = localTime(23, 59, 59);
        const auto afterMidnight = beforeMidnight + std::chrono::seconds(2);
        int calls = 0;
        WithdrawalService service(f.db, f.config, [&] {
            return calls++ == 0 ? beforeMidnight : afterMidnight;
            });
        const auto entry = service.withdraw(f.client, 1, Money::uah(100));
        EXPECT_EQ(calls, 1);
        EXPECT_NE(localDay(beforeMidnight), localDay(afterMidnight));
        EXPECT_EQ(entry.atUtc, toEpochSeconds(beforeMidnight));
        EXPECT_EQ(entry.localDay, localDay(beforeMidnight));
        EXPECT_EQ(f.db.journal().last(1).front(), entry);
        EXPECT_EQ(f.db.journal().withdrawnOn(f.client, localDay(beforeMidnight)), Money::uah(100));
        EXPECT_EQ(f.db.journal().withdrawnOn(f.client, localDay(afterMidnight)), Money{});
    }

    TEST(FinancialSql, D1DepositPersistsBalanceExactCashAndJournal) {
        Fixture f;
        const auto notes = BanknoteSet{}.with(Denomination::UAH100, 2).with(Denomination::UAH500, 1);
        const auto entry = DepositService(f.db, f.clock).deposit(f.client, 2, notes);
        EXPECT_EQ(balance(f, 1), Money::uah(20'000));
        EXPECT_EQ(balance(f, 2), Money::uah(5'700));
        EXPECT_EQ(f.db.cash().load(), BanknoteSet{}.with(Denomination::UAH100, 202).with(Denomination::UAH500, 1));
        EXPECT_EQ(entry.kind, EntryKind::Deposit);
        EXPECT_EQ(entry.account, std::optional<AccountId>{2});
        EXPECT_EQ(entry.amount, Money::uah(700));
        EXPECT_EQ(entry.cash, notes);
        expectJournal(f, entry, 1);
    }

    TEST(FinancialSql, TR1TransferToActiveOrRetainedRecipientPersistsBothBalances) {
        for (bool retained : {false, true}) {
            Fixture f;
            if (retained) sql(f.db, "UPDATE cards SET status='Retained', failed_attempts=3 WHERE card_number='4000000000000002';");
            TransferService service(f.db, f.clock);
            const auto before = snapshot(f.db);
            const auto preview = service.preview(f.client, 2, f.recipient, Money::uah(100));
            EXPECT_EQ(snapshot(f.db), before);
            EXPECT_EQ(preview.toAccount, 3);
            EXPECT_EQ(preview.recipientName, std::string("Recipient"));
            const auto cash = f.db.cash().load();
            const auto entry = service.execute(f.client, 2, preview);
            EXPECT_EQ(balance(f, 1), Money::uah(20'000));
            EXPECT_EQ(balance(f, 2), Money::uah(4'900));
            EXPECT_EQ(balance(f, 3), Money::uah(600));
            EXPECT_EQ(f.db.cash().load(), cash);
            EXPECT_EQ(entry.kind, EntryKind::Transfer);
            EXPECT_EQ(entry.account, std::optional<AccountId>{2});
            EXPECT_EQ(entry.target, std::optional<AccountId>{3});
            EXPECT_EQ(entry.amount, Money::uah(100));
            expectJournal(f, entry, 1);
        }
    }

    TEST(FinancialSql, TR2InvalidRecipientsAmountsAndPreviewLeaveDatabaseUnchanged) {
        Fixture f;
        TransferService service(f.db, f.clock);
        const auto amount = Money::uah(100);
        expectRejected(f, ErrorReason::SelfTransfer, [&] { return service.preview(f.client, 1, f.client, amount); });
        expectRejected(f, ErrorReason::UnknownCard, [&] { return service.preview(f.client, 1, f.service, amount); });
        const auto missing = CardNumber::parse("4000000000000009");
        expectRejected(f, ErrorReason::UnknownCard, [&] { return service.preview(f.client, 1, missing, amount); });
        expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.preview(f.client, 1, f.recipient, Money{}); });
        const auto good = service.preview(f.client, 1, f.recipient, amount);
        auto changed = good;
        changed.toAccount = 2;
        expectRejected(f, ErrorReason::InvalidPreview, [&] { return service.execute(f.client, 1, changed); });
        changed = good;
        changed.recipientName = "Another name";
        expectRejected(f, ErrorReason::InvalidPreview, [&] { return service.execute(f.client, 1, changed); });
        changed = good;
        changed.toCard = f.service;
        expectRejected(f, ErrorReason::InvalidPreview, [&] { return service.execute(f.client, 1, changed); });
        changed = good;
        changed.toCard = f.client;
        expectRejected(f, ErrorReason::SelfTransfer, [&] { return service.execute(f.client, 1, changed); });
        changed = good;
        changed.amount = Money{};
        expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.execute(f.client, 1, changed); });
    }

    TEST(FinancialSql, TR2InsufficientFundsAndRecipientOverflowDoNotPartiallyTransfer) {
        Fixture f;
        TransferService service(f.db, f.clock);
        const auto preview = service.preview(f.client, 1, f.recipient, Money::uah(100));
        setBalance(f, 1, Money{});
        expectRejected(f, ErrorReason::InsufficientFunds, [&] { return service.execute(f.client, 1, preview); });
        setBalance(f, 1, Money::uah(1'000));
        setBalance(f, 3, Money::ofKopiykas((std::numeric_limits<std::int64_t>::max)()));
        expectRejected(f, ErrorReason::Overflow, [&] { return service.execute(f.client, 1, preview); });
    }

    TEST(FinancialSql, P1MobileAndCharityPaymentsRoundTripOptionalFields) {
        Fixture f;
        PaymentService service(f.db, f.clock);
        const auto cash = f.db.cash().load();
        const auto mobile = service.pay(f.client, 2, 1, "0501234567", Money::ofKopiykas(10'025));
        EXPECT_EQ(mobile.kind, EntryKind::Payment);
        EXPECT_EQ(mobile.actor, f.client);
        EXPECT_EQ(mobile.account, std::optional<AccountId>{2});
        EXPECT_EQ(mobile.payeeId, std::optional<std::int64_t>{1});
        EXPECT_EQ(mobile.requisite, std::string("0501234567"));
        EXPECT_EQ(mobile.amount, Money::ofKopiykas(10'025));
        EXPECT_EQ(balance(f, 2), Money::ofKopiykas(489'975));
        expectJournal(f, mobile, 1);
        const auto charity = service.pay(f.client, 2, 2, "", balance(f, 2));
        EXPECT_EQ(charity.payeeId, std::optional<std::int64_t>{2});
        EXPECT_TRUE(charity.requisite.empty());
        EXPECT_EQ(balance(f, 2), Money{});
        EXPECT_EQ(balance(f, 1), Money::uah(20'000));
        EXPECT_EQ(f.db.cash().load(), cash);
        EXPECT_EQ(f.db.journal().withdrawnOn(f.client, localDay(f.now)), Money{});
        expectJournal(f, charity, 2);
    }

    TEST(FinancialSql, P1InvalidPaymentsPreserveAllTables) {
        Fixture f;
        PaymentService service(f.db, f.clock);
        expectRejected(f, ErrorReason::UnknownPayee, [&] { return service.pay(f.client, 1, 999, "", Money::uah(100)); });
        for (std::string_view phone : {"", "050123456", "05012345678", "+380501234567", "050123456a"}) {
            expectRejected(f, ErrorReason::InvalidRequisite, [&] { return service.pay(f.client, 1, 1, phone, Money::uah(100)); });
        }
        expectRejected(f, ErrorReason::InvalidRequisite, [&] { return service.pay(f.client, 1, 2, " ", Money::uah(100)); });
        expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.pay(f.client, 1, 2, "", Money{}); });
        expectRejected(f, ErrorReason::InsufficientFunds, [&] { return service.pay(f.client, 1, 2, "", Money::uah(20'001)); });
    }

    TEST(FinancialSql, SV1LoadUnloadAndInventoryKeepClientBalances) {
        Fixture f;
        CashService service(f.db, f.clock);
        const auto before = snapshot(f.db);
        const auto initial = service.inventory(f.service);
        EXPECT_EQ(snapshot(f.db), before);
        const auto notes = BanknoteSet{}.with(Denomination::UAH200, 3).with(Denomination::UAH2000, 2);
        const auto loaded = service.load(f.service, notes);
        EXPECT_EQ(service.inventory(f.service), initial.plus(notes));
        EXPECT_EQ(loaded.kind, EntryKind::CashLoaded);
        EXPECT_EQ(loaded.actor, f.service);
        EXPECT_EQ(loaded.amount, Money::uah(4'600));
        EXPECT_EQ(loaded.cash, notes);
        expectJournal(f, loaded, 1);
        const auto unloaded = service.unload(f.service, notes);
        EXPECT_EQ(service.inventory(f.service), initial);
        EXPECT_EQ(unloaded.kind, EntryKind::CashUnloaded);
        EXPECT_EQ(unloaded.cash, notes);
        EXPECT_EQ(unloaded.amount, Money::uah(4'600));
        expectJournal(f, unloaded, 2);
        EXPECT_EQ(balance(f, 1), Money::uah(20'000));
        EXPECT_EQ(balance(f, 2), Money::uah(5'000));
        EXPECT_EQ(balance(f, 3), Money::uah(500));
    }

    TEST(FinancialSql, SV1ShortageOverflowAndEmptySetsAreRejected) {
        Fixture f;
        CashService service(f.db, f.clock);
        expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.unload(f.service, BanknoteSet{}.with(Denomination::UAH100, 201)); });
        expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.load(f.service, BanknoteSet{}); });
        expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.unload(f.service, BanknoteSet{}); });
        setCash(f, BanknoteSet{}.with(Denomination::UAH100, (std::numeric_limits<std::uint32_t>::max)()));
        expectRejected(f, ErrorReason::Overflow, [&] { return service.load(f.service, f.notes); });
    }

    TEST(FinancialSql, SV2EmptyJournalHasZeroTotals) {
        Fixture f;
        MonitoringService monitor(f.db, f.config);
        const auto before = snapshot(f.db);
        EXPECT_TRUE(monitor.recent(f.service).empty());
        EXPECT_EQ(monitor.totalLoaded(f.service), Money{});
        EXPECT_EQ(monitor.totalDeposited(f.service), Money{});
        EXPECT_EQ(monitor.totalWithdrawn(f.service), Money{});
        EXPECT_EQ(snapshot(f.db), before);
    }

    TEST(FinancialSql, SV2TailUsesDescendingIdsAndTotalsUseWholeHistory) {
        Fixture f;
        std::vector<JournalEntry> history;
        for (int i = 0; i < 25; ++i) {
            history.push_back(f.run(4));
            f.now -= std::chrono::seconds(1);
        }
        history.push_back(f.run(5));
        history.push_back(f.run(1));
        history.push_back(f.run(0));
        history.push_back(f.run(2));
        history.push_back(f.run(3));
        MonitoringService monitor(f.db, f.config);
        const auto before = snapshot(f.db);
        const auto recent = monitor.recent(f.service);
        ASSERT_EQ(recent.size(), 20U);
        for (std::size_t i = 0; i < recent.size(); ++i) {
            EXPECT_EQ(recent[i], history[history.size() - 1 - i]);
        }
        EXPECT_EQ(monitor.totalLoaded(f.service), Money::uah(2'500));
        EXPECT_EQ(monitor.totalDeposited(f.service), Money::uah(100));
        EXPECT_EQ(monitor.totalWithdrawn(f.service), Money::uah(100));
        EXPECT_EQ(snapshot(f.db), before);
    }
    TEST(FinancialSql, TR2ExecuteRechecksRecipientAfterPreview) {
        Fixture f;
        TransferService service(f.db, f.clock);
        const auto preview = service.preview(f.client, 1, f.recipient, Money::uah(100));
        sql(f.db, "UPDATE cards SET holder_name='Changed recipient' WHERE card_number='4000000000000002';");
        expectRejected(f, ErrorReason::InvalidPreview, [&] { return service.execute(f.client, 1, preview); });
        sql(f.db, "UPDATE cards SET holder_name='Recipient' WHERE card_number='4000000000000002';");
        sql(f.db, "UPDATE accounts SET is_primary=0 WHERE account_id=3;");
        expectRejected(f, ErrorReason::InvalidPreview, [&] { return service.execute(f.client, 1, preview); });
        expectRejected(f, ErrorReason::UnknownAccount, [&] { return service.preview(f.client, 1, f.recipient, Money::uah(100)); });
    }

    TEST(FinancialSql, D1EmptyDepositAndOverflowsLeaveAllTablesUnchanged) {
        Fixture f;
        DepositService service(f.db, f.clock);
        expectRejected(f, ErrorReason::InvalidAmount, [&] { return service.deposit(f.client, 1, BanknoteSet{}); });
        setBalance(f, 1, Money::ofKopiykas((std::numeric_limits<std::int64_t>::max)()));
        expectRejected(f, ErrorReason::Overflow, [&] { return service.deposit(f.client, 1, f.notes); });
        setBalance(f, 1, Money{});
        setCash(f, BanknoteSet{}.with(Denomination::UAH100, (std::numeric_limits<std::uint32_t>::max)()));
        expectRejected(f, ErrorReason::Overflow, [&] { return service.deposit(f.client, 1, f.notes); });
    }

}
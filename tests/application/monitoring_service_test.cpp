#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include "application/cash_service.hpp"
#include "application/deposit_service.hpp"
#include "application/monitoring_service.hpp"
#include "application/withdrawal_service.hpp"
#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using atm::application::CashService;
    using atm::application::DepositService;
    using atm::application::MonitoringService;
    using atm::application::WithdrawalService;
    using atm::domain::AtmConfig;
    using atm::domain::BanknoteSet;
    using atm::domain::CardNumber;
    using atm::domain::Denomination;
    using atm::domain::EntryKind;
    using atm::domain::ErrorReason;
    using atm::domain::JournalEntry;
    using atm::domain::Money;
    using atm::domain::Account;
    using atm::domain::Card;
    using atm::domain::CardKind;
    using atm::domain::CardStatus;
    using atm::test_support::TestStorage;

    static_assert(!std::is_constructible_v<MonitoringService, TestStorage&, AtmConfig&&>);

    constexpr std::int64_t kInt64Max = (std::numeric_limits<std::int64_t>::max)();

    struct Fixture {
        TestStorage storage;
        const CardNumber operatorCard = CardNumber::parse("4000000000000001");
        const CardNumber client = CardNumber::parse("4000000000000002");
        const CardNumber retained = CardNumber::parse("4000000000000003");
        const atm::domain::TimePoint now = atm::domain::fromEpochSeconds(1'768'471'200);
        AtmConfig config;
        MonitoringService service;

        explicit Fixture(AtmConfig settings = {})
            : config(settings), service(storage, config) {
            seed();
        }

        static AtmConfig makeConfig(std::size_t tailSize) {
            AtmConfig result;
            result.journalTailSize = tailSize;
            return result;
        }

        void seed() {
            storage.cards().save(Card(operatorCard, "Operator", "hash", CardKind::Service));
            storage.cards().save(Card(client, "Client", "hash", CardKind::Client));
            storage.cards().save(Card(retained, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));
            storage.accounts().save(Account(1, client, Money::uah(30'000), true));
            storage.accounts().save(Account(2, client, Money::uah(20'000)));
            BanknoteSet cash;
            for (const auto denomination : atm::domain::kAllDenominations) {
                cash = cash.with(denomination, 50);
            }
            storage.cash().save(cash);
        }

        BanknoteSet cash() { return storage.cash().load(); }
        std::vector<JournalEntry> journal() { return storage.journal().last(100); }
    };

    // Seed the in-memory journal directly to isolate ordering and aggregation.
    JournalEntry append(Fixture& f, EntryKind kind, Money amount, std::int64_t secondsEarlier = 0) {
        JournalEntry entry{};
        entry.kind = kind;
        entry.actor = f.client;
        entry.localDay = atm::domain::localDay(f.now);
        entry.atUtc = atm::domain::toEpochSeconds(f.now) - secondsEarlier;
        entry.amount = amount;
        return f.storage.journal().append(entry);
    }

    TEST(SV2Monitoring, ConstructorRejectsZeroTailSize) {
        TestStorage storage;
        AtmConfig bad;
        bad.journalTailSize = 0;
        EXPECT_DOMAIN_ERROR((void)MonitoringService(storage, bad), ErrorReason::InvalidData);
    }

    TEST(SV2Monitoring, RecentOnEmptyJournalIsEmpty) {
        Fixture f;
        EXPECT_EQ(f.service.recent(f.operatorCard).size(), 0U);
    }

    TEST(SV2Monitoring, FewerEntriesThanTailSizeReturnsAllNewestFirst) {
        Fixture f;
        for (int i = 0; i < 3; ++i) {
            (void)append(f, EntryKind::Deposit, Money::uah(10));
        }
        const auto recent = f.service.recent(f.operatorCard);
        ASSERT_EQ(recent.size(), 3U);
        EXPECT_EQ(recent[0].id, 3);
        EXPECT_EQ(recent[1].id, 2);
        EXPECT_EQ(recent[2].id, 1);
    }

    TEST(SV2Monitoring, TwentyFiveEntriesGiveLastTwentyByIdDescending) {
        Fixture f;
        for (int i = 0; i < 25; ++i) {
            (void)append(f, EntryKind::Deposit, Money::uah(1), i);
        }
        const auto recent = f.service.recent(f.operatorCard);
        ASSERT_EQ(recent.size(), 20U);
        EXPECT_EQ(recent.front().id, 25);
        EXPECT_EQ(recent.back().id, 6);
        EXPECT_TRUE(recent.front().atUtc < recent.back().atUtc);
        for (std::size_t i = 1; i < recent.size(); ++i) {
            EXPECT_TRUE(recent[i - 1].id > recent[i].id);
        }
    }

    TEST(SV2Monitoring, ExactlyTailSizeEntriesAreAllReturned) {
        Fixture f;
        for (int i = 0; i < 20; ++i) {
            (void)append(f, EntryKind::Deposit, Money::uah(1));
        }
        EXPECT_EQ(f.service.recent(f.operatorCard).size(), 20U);
    }

    TEST(SV2Monitoring, TailSizeComesFromConfig) {
        Fixture f(Fixture::makeConfig(5));
        for (int i = 0; i < 12; ++i) {
            (void)append(f, EntryKind::Deposit, Money::uah(1));
        }
        const auto recent = f.service.recent(f.operatorCard);
        ASSERT_EQ(recent.size(), 5U);
        EXPECT_EQ(recent.front().id, 12);
        EXPECT_EQ(recent.back().id, 8);
    }

    TEST(SV2Monitoring, JournalTailLimitDoesNotLimitTotalLoaded) {
        Fixture f(Fixture::makeConfig(1));
        CashService cash(f.storage, [&f] { return f.now; });
        (void)cash.load(f.operatorCard, BanknoteSet{}.with(Denomination::UAH500, 1));
        const auto latest = cash.load(f.operatorCard, BanknoteSet{}.with(Denomination::UAH1000, 1));

        const auto recent = f.service.recent(f.operatorCard);
        ASSERT_EQ(recent.size(), std::size_t{ 1 });
        EXPECT_EQ(recent.front(), latest);
        EXPECT_EQ(f.service.totalLoaded(f.operatorCard), Money::uah(1'500));
    }

    TEST(SV2Monitoring, TotalsOnEmptyJournalAreZero) {
        Fixture f;
        EXPECT_EQ(f.service.totalLoaded(f.operatorCard), Money{});
        EXPECT_EQ(f.service.totalDeposited(f.operatorCard), Money{});
        EXPECT_EQ(f.service.totalWithdrawn(f.operatorCard), Money{});
    }

    TEST(SV2Monitoring, EachTotalCountsOnlyItsOwnKind) {
        Fixture f;
        (void)append(f, EntryKind::CashLoaded, Money::uah(1'000));
        (void)append(f, EntryKind::CashLoaded, Money::uah(500));
        (void)append(f, EntryKind::Deposit, Money::uah(200));
        (void)append(f, EntryKind::Withdrawal, Money::uah(300));
        (void)append(f, EntryKind::Withdrawal, Money::uah(100));
        (void)append(f, EntryKind::Transfer, Money::uah(9'999));
        (void)append(f, EntryKind::Payment, Money::uah(8'888));
        (void)append(f, EntryKind::CashUnloaded, Money::uah(7'777));

        EXPECT_EQ(f.service.totalLoaded(f.operatorCard), Money::uah(1'500));
        EXPECT_EQ(f.service.totalDeposited(f.operatorCard), Money::uah(200));
        EXPECT_EQ(f.service.totalWithdrawn(f.operatorCard), Money::uah(400));
    }

    TEST(SV2Monitoring, UnloadingDoesNotReduceTotalLoaded) {
        Fixture f;
        CashService cash(f.storage, [&f] { return f.now; });
        const BanknoteSet notes = BanknoteSet{}.with(Denomination::UAH1000, 10);
        (void)cash.load(f.operatorCard, notes);
        (void)cash.unload(f.operatorCard, notes);
        EXPECT_EQ(f.service.totalLoaded(f.operatorCard), Money::uah(10'000));
    }

    TEST(SV2Monitoring, TotalsReflectRealOperations) {
        Fixture f;
        CashService cash(f.storage, [&f] { return f.now; });
        DepositService deposit(f.storage, [&f] { return f.now; });
        WithdrawalService withdrawal(f.storage, f.config, [&f] { return f.now; });

        (void)cash.load(f.operatorCard, BanknoteSet{}.with(Denomination::UAH500, 4));
        (void)deposit.deposit(f.client, 1,
            BanknoteSet{}.with(Denomination::UAH200, 3));
        (void)withdrawal.withdraw(f.client, 1, Money::uah(1'300));
        (void)withdrawal.withdraw(f.client, 2, Money::uah(200));

        EXPECT_EQ(f.service.totalLoaded(f.operatorCard), Money::uah(2'000));
        EXPECT_EQ(f.service.totalDeposited(f.operatorCard), Money::uah(600));
        EXPECT_EQ(f.service.totalWithdrawn(f.operatorCard), Money::uah(1'500));
        EXPECT_EQ(f.service.recent(f.operatorCard).size(), 4U);
    }

    TEST(SV2Monitoring, TotalOverflowIsReportedNotWrapped) {
        Fixture f;
        (void)append(f, EntryKind::CashLoaded, Money::ofKopiykas(kInt64Max / 2 + 1));
        (void)append(f, EntryKind::CashLoaded, Money::ofKopiykas(kInt64Max / 2 + 1));
        EXPECT_DOMAIN_ERROR((void)f.service.totalLoaded(f.operatorCard), ErrorReason::Overflow);
    }

    TEST(SV2Monitoring, ReadingChangesNothingAndAddsNoEntries) {
        Fixture f;
        (void)append(f, EntryKind::Deposit, Money::uah(10));
        const auto before = f.cash();
        const auto journalBefore = f.journal();
        (void)f.service.recent(f.operatorCard);
        (void)f.service.totalLoaded(f.operatorCard);
        (void)f.service.totalDeposited(f.operatorCard);
        (void)f.service.totalWithdrawn(f.operatorCard);
        EXPECT_EQ(f.cash(), before);
        EXPECT_EQ(f.journal(), journalBefore);
        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(f.storage.rollbacks, 0);
        EXPECT_EQ(f.journal().size(), 1U);
    }

    TEST(M2Monitoring, ClientAndRetainedCardsAreAccessDeniedForEveryMethod) {
        Fixture f;
        for (const CardNumber* card : { &f.client, &f.retained }) {
            EXPECT_DOMAIN_ERROR((void)f.service.recent(*card), ErrorReason::AccessDenied);
            EXPECT_DOMAIN_ERROR((void)f.service.totalLoaded(*card), ErrorReason::AccessDenied);
            EXPECT_DOMAIN_ERROR((void)f.service.totalDeposited(*card), ErrorReason::AccessDenied);
            EXPECT_DOMAIN_ERROR((void)f.service.totalWithdrawn(*card), ErrorReason::AccessDenied);
        }
    }

    TEST(M2Monitoring, UnknownCardIsRejected) {
        Fixture f;
        const CardNumber unknown = CardNumber::parse("9000000000009999");
        EXPECT_DOMAIN_ERROR((void)f.service.recent(unknown), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.totalLoaded(unknown), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.totalDeposited(unknown), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.totalWithdrawn(unknown), ErrorReason::UnknownCard);
    }

}  // namespace

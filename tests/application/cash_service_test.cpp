#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "application/cash_service.hpp"
#include "application/withdrawal_service.hpp"
#include "domain/banknote_set.hpp"
#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using atm::application::CashService;
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
    using atm::test_support::InjectedStorageFailure;

    struct Fixture {
        TestStorage storage;
        const CardNumber operatorCard = CardNumber::parse("4000000000000001");
        const CardNumber client = CardNumber::parse("4000000000000002");
        const CardNumber retained = CardNumber::parse("4000000000000003");
        const atm::domain::TimePoint now = atm::domain::fromEpochSeconds(1'768'471'200);
        int clockCalls = 0;
        CashService service{ storage, [this] {
            ++clockCalls;
            return now;
        } };
        Fixture() {
            storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
            storage.seed(Card(client, "Client", "hash", CardKind::Client));
            storage.seed(Card(retained, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));
            storage.seed(Account(1, client, Money::uah(30'000), true));
            storage.seed(Account(2, client, Money::uah(20'000)));
            BanknoteSet cash;
            for (const auto denomination : atm::domain::kAllDenominations) {
                cash = cash.with(denomination, 50);
            }
            storage.seed(cash);
        }

        BanknoteSet cash() { return storage.cash().load(); }
        std::vector<JournalEntry> journal() { return storage.journal().last(100); }
    };

    BanknoteSet notes(std::uint32_t n100, std::uint32_t n200, std::uint32_t n500, std::uint32_t n1000,
        std::uint32_t n2000) {
        return BanknoteSet{}
            .with(Denomination::UAH100, n100)
            .with(Denomination::UAH200, n200)
            .with(Denomination::UAH500, n500)
            .with(Denomination::UAH1000, n1000)
            .with(Denomination::UAH2000, n2000);
    }

    TEST(SV1Cash, ConstructorRejectsEmptyClock) {
        TestStorage storage;
        EXPECT_DOMAIN_ERROR((void)CashService(storage, atm::domain::Clock{}), ErrorReason::InvalidData);
    }

    TEST(SV1Cash, InventoryReturnsExactCashAndChangesNothing) {
        Fixture f;
        f.storage.seed(notes(1, 2, 3, 4, 5));
        const auto before = f.cash();
        const auto journalBefore = f.journal();
        EXPECT_EQ(f.service.inventory(f.operatorCard), notes(1, 2, 3, 4, 5));
        EXPECT_EQ(f.cash(), before);
        EXPECT_EQ(f.journal(), journalBefore);
        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(f.journal().size(), 0U);
        EXPECT_EQ(f.clockCalls, 0);
    }

    TEST(SV1Cash, LoadAddsExactNotesAndWritesJournal) {
        Fixture f;
        const BanknoteSet loaded = notes(10, 0, 5, 0, 2);
        const JournalEntry entry = f.service.load(f.operatorCard, loaded);

        EXPECT_EQ(f.cash(), notes(50, 50, 50, 50, 50).plus(loaded));
        EXPECT_EQ(f.cash().count(Denomination::UAH100), 60U);
        EXPECT_EQ(f.cash().count(Denomination::UAH200), 50U);

        EXPECT_EQ(entry.id, 1);
        EXPECT_EQ(entry.kind, EntryKind::CashLoaded);
        EXPECT_EQ(entry.actor, f.operatorCard);
        EXPECT_FALSE(entry.account.has_value());
        EXPECT_FALSE(entry.target.has_value());
        EXPECT_FALSE(entry.subjectCard.has_value());
        EXPECT_FALSE(entry.payeeId.has_value());
        EXPECT_EQ(entry.requisite, "");
        EXPECT_EQ(entry.amount, Money::uah(7'500));
        EXPECT_EQ(entry.cash, loaded);
        EXPECT_EQ(entry.cash.total(), entry.amount);
        EXPECT_EQ(entry.localDay, atm::domain::localDay(f.now));
        EXPECT_EQ(entry.atUtc, atm::domain::toEpochSeconds(f.now));

        const auto journal = f.journal();
        ASSERT_EQ(journal.size(), 1U);
        EXPECT_EQ(journal[0], entry);
        EXPECT_EQ(f.clockCalls, 1);
    }

    TEST(SV1Cash, LoadDoesNotTouchClientBalances) {
        Fixture f;
        (void)f.service.load(f.operatorCard, notes(1, 1, 1, 1, 1));
        EXPECT_EQ(f.storage.accounts().find(1).value().balance(), Money::uah(30'000));
        EXPECT_EQ(f.storage.accounts().find(2).value().balance(), Money::uah(20'000));
    }

    TEST(SV1Cash, UnloadRemovesExactNotesAndWritesJournal) {
        Fixture f;
        const BanknoteSet removed = notes(0, 20, 0, 5, 0);
        const JournalEntry entry = f.service.unload(f.operatorCard, removed);

        EXPECT_EQ(f.cash(), notes(50, 50, 50, 50, 50).minus(removed));
        EXPECT_EQ(f.cash().count(Denomination::UAH200), 30U);
        EXPECT_EQ(f.cash().count(Denomination::UAH1000), 45U);
        EXPECT_EQ(entry.kind, EntryKind::CashUnloaded);
        EXPECT_EQ(entry.actor, f.operatorCard);
        EXPECT_FALSE(entry.account.has_value());
        EXPECT_EQ(entry.amount, Money::uah(9'000));
        EXPECT_EQ(entry.cash, removed);
        const auto journal = f.journal();
        ASSERT_EQ(journal.size(), std::size_t{ 1 });
        EXPECT_EQ(journal.front(), entry);
        EXPECT_EQ(f.clockCalls, 1);
        EXPECT_EQ(f.storage.commits, 1);
    }

    TEST(SV1Cash, UnloadEverythingLeavesEmptyCash) {
        Fixture f;
        (void)f.service.unload(f.operatorCard, notes(50, 50, 50, 50, 50));
        EXPECT_EQ(f.cash(), BanknoteSet{});
        EXPECT_EQ(f.service.inventory(f.operatorCard), BanknoteSet{});
    }

    TEST(SV1Cash, LoadThenUnloadSameSetRestoresCash) {
        Fixture f;
        const BanknoteSet original = f.cash();
        const BanknoteSet set = notes(3, 4, 5, 6, 7);
        (void)f.service.load(f.operatorCard, set);
        (void)f.service.unload(f.operatorCard, set);
        EXPECT_EQ(f.cash(), original);
        EXPECT_EQ(f.journal().size(), 2U);
    }

    TEST(SV1Cash, UnloadShortageIsRejectedWithoutPartialChange) {
        Fixture f;
        const BanknoteSet tooMuch = notes(10, 10, 51, 0, 0);
        const auto before = f.cash();
        const auto journalBefore = f.journal();
        EXPECT_DOMAIN_ERROR((void)f.service.unload(f.operatorCard, tooMuch), ErrorReason::InvalidAmount);
        EXPECT_EQ(f.cash(), before);
        EXPECT_EQ(f.journal(), journalBefore);
        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(f.journal().size(), 0U);
    }

    TEST(SV1Cash, LoadCountOverflowIsRejectedWithoutChange) {
        Fixture f;
        constexpr std::uint32_t kMax = (std::numeric_limits<std::uint32_t>::max)();
        f.storage.seed(BanknoteSet{}.with(Denomination::UAH100, kMax));
        const auto before = f.cash();
        const auto journalBefore = f.journal();
        EXPECT_DOMAIN_ERROR((void)f.service.load(f.operatorCard, notes(1, 0, 0, 0, 0)), ErrorReason::Overflow);
        EXPECT_EQ(f.cash(), before);
        EXPECT_EQ(f.journal(), journalBefore);
        EXPECT_EQ(f.storage.commits, 0);
    }

    TEST(SV1Cash, EmptySetIsRejectedBeforeStorageAndClock) {
        Fixture f;
        const auto before = f.cash();
        const auto journalBefore = f.journal();
        EXPECT_DOMAIN_ERROR((void)f.service.load(f.operatorCard, BanknoteSet{}), ErrorReason::InvalidAmount);
        EXPECT_DOMAIN_ERROR((void)f.service.unload(f.operatorCard, BanknoteSet{}), ErrorReason::InvalidAmount);
        EXPECT_EQ(f.cash(), before);
        EXPECT_EQ(f.journal(), journalBefore);
        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(f.clockCalls, 0);
        EXPECT_EQ(f.storage.rollbacks, 0);
    }

    TEST(SV1Cash, ClockIsNotReadWhenNotesAreUnavailable) {
        Fixture f;
        EXPECT_DOMAIN_ERROR((void)f.service.unload(f.operatorCard, notes(0, 0, 0, 0, 99)), ErrorReason::InvalidAmount);
        EXPECT_EQ(f.clockCalls, 0);
    }

    TEST(SV1Cash, LoadedNotesBecomeAvailableForWithdrawal) {
        Fixture f;
        f.storage.seed(BanknoteSet{});
        (void)f.service.load(f.operatorCard, notes(0, 0, 2, 0, 0));
        AtmConfig config;
        WithdrawalService withdrawal(f.storage, config, [&f] { return f.now; });
        const JournalEntry out = withdrawal.withdraw(f.client, 1, Money::uah(1000));
        EXPECT_EQ(out.cash, notes(0, 0, 2, 0, 0));
        EXPECT_EQ(f.cash(), BanknoteSet{});
    }

    TEST(M2Cash, ClientAndRetainedCardsAreAccessDeniedForEveryMethod) {
        Fixture f;
        const auto before = f.cash();
        const auto journalBefore = f.journal();
        for (const CardNumber* card : { &f.client, &f.retained }) {
            EXPECT_DOMAIN_ERROR((void)f.service.inventory(*card), ErrorReason::AccessDenied);
            EXPECT_DOMAIN_ERROR((void)f.service.load(*card, notes(1, 0, 0, 0, 0)), ErrorReason::AccessDenied);
            EXPECT_DOMAIN_ERROR((void)f.service.unload(*card, notes(1, 0, 0, 0, 0)), ErrorReason::AccessDenied);
        }
        EXPECT_EQ(f.cash(), before);
        EXPECT_EQ(f.journal(), journalBefore);
        EXPECT_EQ(f.storage.commits, 0);
    }

    TEST(M2Cash, UnknownCardIsRejected) {
        Fixture f;
        const CardNumber unknown = CardNumber::parse("9000000000009999");
        EXPECT_DOMAIN_ERROR((void)f.service.inventory(unknown), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.load(unknown, notes(1, 0, 0, 0, 0)), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.unload(unknown, notes(1, 0, 0, 0, 0)), ErrorReason::UnknownCard);
    }

    TEST(SV1Cash, X1RestoresCashAndJournalOnAppendAndCommitFailure) {
        for (const auto failure : { TestStorage::Failure::Append, TestStorage::Failure::Commit }) {
            for (const bool isLoad : { true, false }) {
                Fixture f;
                const auto change = notes(1, 0, 1, 0, 0);
                (void)f.service.load(f.operatorCard, change);
                const auto before = f.cash();
                const auto journalBefore = f.journal();
                f.storage.failure = failure;
                const auto run = [&] {
                    return isLoad ? f.service.load(f.operatorCard, change)
                        : f.service.unload(f.operatorCard, change);
                    };

                EXPECT_THROW((void)run(), InjectedStorageFailure);
                EXPECT_EQ(f.cash(), before);
                EXPECT_EQ(f.journal(), journalBefore);
                EXPECT_EQ(f.storage.commits, 1);
                EXPECT_EQ(f.storage.rollbacks, 1);

                f.storage.failure = TestStorage::Failure::None;
                const auto saved = run();
                EXPECT_EQ(f.cash(), isLoad ? before.plus(change) : before.minus(change));
                EXPECT_EQ(f.storage.commits, 2);
                const auto journal = f.journal();
                ASSERT_EQ(journal.size(), std::size_t{ 2 });
                EXPECT_EQ(journal.front(), saved);
            }
        }
    }

}  // namespace
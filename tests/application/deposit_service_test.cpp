#include "application/deposit_service.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/enums.hpp"
#include "domain/money.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using namespace atm::domain;
    using atm::application::DepositService;
    using atm::test_support::InjectedStorageFailure;
    using atm::test_support::TestStorage;

    struct Fixture {
        TestStorage storage;
        const CardNumber client = CardNumber::parse("4000000000000001");
        const CardNumber other = CardNumber::parse("4000000000000002");
        const TimePoint now = fromEpochSeconds(1'768'471'200);
        int clockCalls = 0;
        DepositService service{ storage, [this] {
            ++clockCalls;
            return now;
        } };

        Fixture() {
            storage.cards().save(Card(client, "Client", "hash", CardKind::Client));
            storage.cards().save(Card(other, "Other client", "hash", CardKind::Client));
            storage.accounts().save(Account(1, client, Money::uah(2'000), true));
            storage.accounts().save(Account(2, client, Money::uah(1'000)));
            storage.accounts().save(Account(3, other, Money::uah(3'000), true));
            storage.cash().save(BanknoteSet{}.with(Denomination::UAH100, 3)
                .with(Denomination::UAH500, 2));
        }

        Money balance(AccountId id) {
            return storage.accounts().find(id).value().balance();
        }

        void expectUnchanged(AccountId id, Money initialBalance, const BanknoteSet& initialCash) {
            EXPECT_EQ(balance(id), initialBalance);
            EXPECT_EQ(storage.cash().load(), initialCash);
            EXPECT_TRUE(storage.journal().last(1).empty());
            EXPECT_EQ(storage.commits, 0);
        }
    };

    TEST(DepositServiceTest, ConstructorRejectsEmptyClock) {
        TestStorage storage;
        EXPECT_DOMAIN_ERROR((void)DepositService(storage, Clock{}), ErrorReason::InvalidData);
    }

    TEST(DepositServiceTest, ConfirmedDepositCreditsSelectedAccountAndStoresExactNotes) {
        Fixture f;
        const auto notes = BanknoteSet{}.with(Denomination::UAH100, 2)
            .with(Denomination::UAH200, 1);
        const auto entry = f.service.deposit(f.client, 2, notes);

        EXPECT_EQ(f.balance(1), Money::uah(2'000));
        EXPECT_EQ(f.balance(2), Money::uah(1'400));
        EXPECT_EQ(f.balance(3), Money::uah(3'000));
        EXPECT_EQ(f.storage.cash().load(), BanknoteSet{}.with(Denomination::UAH100, 5)
            .with(Denomination::UAH200, 1)
            .with(Denomination::UAH500, 2));
        EXPECT_TRUE(entry.id > 0);
        EXPECT_EQ(entry.kind, EntryKind::Deposit);
        EXPECT_EQ(entry.actor, f.client);
        EXPECT_EQ(entry.account, std::optional<AccountId>{2});
        EXPECT_EQ(entry.amount, Money::uah(400));
        EXPECT_EQ(entry.cash, notes);
        EXPECT_EQ(entry.atUtc, 1'768'471'200);
        EXPECT_EQ(entry.localDay, localDay(f.now));
        EXPECT_EQ(f.clockCalls, 1);
        EXPECT_EQ(f.storage.commits, 1);
        EXPECT_EQ(f.storage.rollbacks, 0);

        const auto journal = f.storage.journal().last(2);
        ASSERT_EQ(journal.size(), std::size_t{ 1 });
        EXPECT_EQ(journal.front(), entry);
    }

    TEST(DepositServiceTest, RejectsEmptyDepositWithoutChangingState) {
        Fixture f;
        const auto initialCash = f.storage.cash().load();
        EXPECT_DOMAIN_ERROR((void)f.service.deposit(f.client, 2, BanknoteSet{}),
            ErrorReason::InvalidAmount);
        f.expectUnchanged(2, Money::uah(1'000), initialCash);
        EXPECT_EQ(f.clockCalls, 0);
    }

    TEST(DepositServiceTest, RejectsMissingCardMissingAccountAndWrongOwner) {
        Fixture f;
        const auto initialCash = f.storage.cash().load();
        const auto notes = BanknoteSet{}.with(Denomination::UAH100, 1);
        const auto missing = CardNumber::parse("4000000000000009");

        EXPECT_DOMAIN_ERROR((void)f.service.deposit(missing, 2, notes), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.deposit(f.client, 9, notes), ErrorReason::UnknownAccount);
        EXPECT_DOMAIN_ERROR((void)f.service.deposit(f.client, 3, notes), ErrorReason::AccessDenied);
        EXPECT_EQ(f.balance(2), Money::uah(1'000));
        f.expectUnchanged(3, Money::uah(3'000), initialCash);
    }

    TEST(DepositServiceTest, RejectsServiceAndRetainedCardsEvenForTheirOwnAccounts) {
        Fixture f;
        const auto initialCash = f.storage.cash().load();
        const auto notes = BanknoteSet{}.with(Denomination::UAH100, 1);
        const auto operatorCard = CardNumber::parse("4000000000000003");
        const auto retained = CardNumber::parse("4000000000000004");
        const auto balance = Money::uah(1'000);
        f.storage.cards().save(Card(operatorCard, "Operator", "hash", CardKind::Service));
        f.storage.cards().save(Card(retained, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));
        f.storage.accounts().save(Account(4, operatorCard, balance));
        f.storage.accounts().save(Account(5, retained, balance));

        EXPECT_DOMAIN_ERROR((void)f.service.deposit(operatorCard, 4, notes), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)f.service.deposit(retained, 5, notes), ErrorReason::AccessDenied);
        EXPECT_EQ(f.balance(4), balance);
        f.expectUnchanged(5, balance, initialCash);
    }

    TEST(DepositServiceTest, RejectsAccountBalanceOverflowWithoutChangingState) {
        Fixture f;
        const auto initialCash = f.storage.cash().load();
        const auto maximum = Money::ofKopiykas((std::numeric_limits<std::int64_t>::max)());
        f.storage.accounts().save(Account(2, f.client, maximum));

        const auto notes = BanknoteSet{}.with(Denomination::UAH100, 1);
        EXPECT_DOMAIN_ERROR((void)f.service.deposit(f.client, 2, notes), ErrorReason::Overflow);
        f.expectUnchanged(2, maximum, initialCash);
    }

    TEST(DepositServiceTest, RejectsBanknoteCountOverflowWithoutChangingState) {
        Fixture f;
        const auto maximum = (std::numeric_limits<std::uint32_t>::max)();
        const auto initialCash = BanknoteSet{}.with(Denomination::UAH100, maximum);
        f.storage.cash().save(initialCash);

        const auto notes = BanknoteSet{}.with(Denomination::UAH100, 1);
        EXPECT_DOMAIN_ERROR((void)f.service.deposit(f.client, 2, notes), ErrorReason::Overflow);
        f.expectUnchanged(2, Money::uah(1'000), initialCash);
    }

    TEST(DepositServiceTest, X1RestoresStateOnAppendAndCommitFailure) {
        for (const auto failure : { TestStorage::Failure::Append, TestStorage::Failure::Commit }) {
            Fixture f;
            const auto notes = BanknoteSet{}.with(Denomination::UAH200, 1);
            (void)f.service.deposit(f.client, 2, notes);
            const auto balanceBefore = f.balance(2);
            const auto cashBefore = f.storage.cash().load();
            const auto journalBefore = f.storage.journal().last(2);
            ASSERT_EQ(journalBefore.size(), std::size_t{ 1 });

            f.storage.failure = failure;
            EXPECT_THROW((void)f.service.deposit(f.client, 2, notes), InjectedStorageFailure);
            EXPECT_EQ(f.balance(2), balanceBefore);
            EXPECT_EQ(f.storage.cash().load(), cashBefore);
            EXPECT_EQ(f.storage.journal().last(2), journalBefore);
            EXPECT_EQ(f.storage.commits, 1);
            EXPECT_EQ(f.storage.rollbacks, 1);

            f.storage.failure = TestStorage::Failure::None;
            (void)f.service.deposit(f.client, 2, notes);
            EXPECT_EQ(f.balance(2), balanceBefore + Money::uah(200));
            EXPECT_EQ(f.storage.commits, 2);
        }
    }

}  // namespace

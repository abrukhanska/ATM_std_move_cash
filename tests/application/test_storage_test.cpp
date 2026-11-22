
#include "support/transactional_test_storage.hpp"
#include "support/mini_test.hpp"

#include <stdexcept>

namespace {

    TEST(TestStorageContract, AccountsAreFilteredAndSortedById) {
        using namespace atm::domain;

        atm::test_support::TestStorage storage;
        const auto client = CardNumber::parse("4000000000000001");
        const auto other = CardNumber::parse("4000000000000002");

        storage.seed(Account(2, client, Money{}, false));
        storage.seed(Account(3, other, Money{}, true));
        storage.seed(Account(1, client, Money{}, true));

        const auto result = storage.accounts().listByCard(client);

        ASSERT_EQ(result.size(), 2U);
        EXPECT_EQ(result[0].id(), 1);
        EXPECT_EQ(result[1].id(), 2);
    }

    TEST(TestStorageContract, AppendRejectsInconsistentEntry) {
        using namespace atm::domain;

        atm::test_support::TestStorage storage;

        JournalEntry entry;
        entry.localDay = "2026-11-22";
        entry.kind = EntryKind::Withdrawal;
        entry.actor = CardNumber::parse("4000000000000001");
        entry.account = 1;
        entry.amount = Money::uah(200);

        storage.begin();

        EXPECT_THROW(
            (void)storage.journal().append(entry),
            std::invalid_argument
        );

        EXPECT_TRUE(storage.journal().last(1).empty());

        storage.rollback();
    }


    TEST(TestStorageContract, SaveRejectsMissingRecord) {
        using namespace atm::domain;

        atm::test_support::TestStorage storage;
        const auto client = CardNumber::parse("4000000000000001");
        const Card card(client, "Client", "hash", CardKind::Client);
        const Account account(1, client, Money::uah(100), true);

        storage.begin();

        EXPECT_THROW(storage.cards().save(card), std::logic_error);
        EXPECT_THROW(storage.accounts().save(account), std::logic_error);
        EXPECT_FALSE(storage.cards().find(client).has_value());
        EXPECT_FALSE(storage.accounts().find(1).has_value());

        storage.rollback();
        EXPECT_EQ(storage.rollbacks, 1);
        EXPECT_EQ(storage.commits, 0);
    }

    TEST(TestStorageContract, WritesRequireTransaction) {
        using namespace atm::domain;

        atm::test_support::TestStorage storage;
        const auto client = CardNumber::parse("4000000000000001");
        const Card card(client, "Client", "hash", CardKind::Client);
        const Account account(1, client, Money::uah(1000), true);
        const auto cash = BanknoteSet{}.with(Denomination::UAH100, 2);

        storage.seed(card);
        storage.seed(account);
        storage.seed(cash);

        JournalEntry entry;
        entry.localDay = "2026-11-22";
        entry.kind = EntryKind::Withdrawal;
        entry.actor = client;
        entry.account = 1;
        entry.amount = Money::uah(100);
        entry.cash = BanknoteSet{}.with(Denomination::UAH100, 1);
        ASSERT_TRUE(isValidJournalEntry(entry));

        EXPECT_THROW(storage.cards().save(card), std::logic_error);
        EXPECT_THROW(storage.accounts().save(account), std::logic_error);
        EXPECT_THROW(storage.cash().save(cash), std::logic_error);
        EXPECT_THROW((void)storage.journal().append(entry), std::logic_error);

        const auto accountAfter = storage.accounts().find(1);
        ASSERT_TRUE(accountAfter.has_value());
        EXPECT_EQ(accountAfter->balance(), Money::uah(1000));
        EXPECT_EQ(storage.cash().load(), cash);
        EXPECT_TRUE(storage.journal().last(10).empty());
        EXPECT_EQ(storage.commits, 0);
        EXPECT_EQ(storage.rollbacks, 0);
    }

    TEST(TestStorageContract, RollbackRestoresSnapshot) {
        using namespace atm::domain;

        atm::test_support::TestStorage storage;
        const auto client = CardNumber::parse("4000000000000001");

        storage.seed(Card(client, "Client", "old-hash", CardKind::Client));
        storage.seed(Account(1, client, Money::uah(1000), true));

        const auto originalCash =
            BanknoteSet{}.with(Denomination::UAH100, 3);
        storage.seed(originalCash);

        const auto cardBefore = storage.cards().find(client);
        const auto accountBefore = storage.accounts().find(1);
        ASSERT_TRUE(cardBefore.has_value());
        ASSERT_TRUE(accountBefore.has_value());

        const auto journalBefore = storage.journal().last(10);

        storage.begin();

        auto changedCard = *cardBefore;
        changedCard.changePinHash("new-hash");
        storage.cards().save(changedCard);

        auto changedAccount = *accountBefore;
        changedAccount.debit(Money::uah(100));
        storage.accounts().save(changedAccount);

        storage.cash().save(
            originalCash.with(Denomination::UAH100, 2)
        );

        JournalEntry entry;
        entry.localDay = "2026-11-22";
        entry.kind = EntryKind::Withdrawal;
        entry.actor = client;
        entry.account = 1;
        entry.amount = Money::uah(100);
        entry.cash = BanknoteSet{}.with(Denomination::UAH100, 1);

        const auto saved = storage.journal().append(entry);
        EXPECT_EQ(saved.id, 1);
        EXPECT_EQ(storage.journal().last(10).size(), 1U);

        storage.rollback();

        const auto cardAfter = storage.cards().find(client);
        const auto accountAfter = storage.accounts().find(1);

        ASSERT_TRUE(cardAfter.has_value());
        ASSERT_TRUE(accountAfter.has_value());

        EXPECT_EQ(cardAfter->pinHash(), cardBefore->pinHash());
        EXPECT_EQ(accountAfter->balance(), accountBefore->balance());
        EXPECT_EQ(storage.cash().load(), originalCash);
        EXPECT_EQ(storage.journal().last(10), journalBefore);
        EXPECT_EQ(storage.rollbacks, 1);
        EXPECT_EQ(storage.commits, 0);
    }


}

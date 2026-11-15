#include <vector>

#include "application/account_service.hpp"
#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/money.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using atm::application::AccountService;
    using atm::domain::Account;
    using atm::domain::Card;
    using atm::domain::CardKind;
    using atm::domain::CardNumber;
    using atm::domain::CardStatus;
    using atm::domain::ErrorReason;
    using atm::domain::Money;
    using atm::test_support::TestStorage;

    TEST(B1Account, ReturnsOwnAccountsAndBalance) {
        TestStorage storage;
        const auto client = CardNumber::parse("4000000000000001");
        const auto otherClient = CardNumber::parse("4000000000000002");

        storage.seed(Card(client, "Alice", "hash", CardKind::Client));
        storage.seed(Card(otherClient, "Bob", "hash", CardKind::Client));

        storage.seed(Account(1, client, Money::uah(12'500), true));
        storage.seed(Account(2, client, Money::uah(3'750), false));
        storage.seed(Account(3, otherClient, Money::uah(99'000), true));

        AccountService service(storage);

        const std::vector<Account> accounts = service.accountsOf(client);
        ASSERT_EQ(accounts.size(), std::size_t{ 2 });
        EXPECT_EQ(accounts[0].id(), 1);
        EXPECT_EQ(accounts[0].owner(), client);
        EXPECT_EQ(accounts[0].balance(), Money::uah(12'500));
        EXPECT_TRUE(accounts[0].isPrimary());

        EXPECT_EQ(accounts[1].id(), 2);
        EXPECT_EQ(accounts[1].owner(), client);
        EXPECT_EQ(accounts[1].balance(), Money::uah(3'750));
        EXPECT_FALSE(accounts[1].isPrimary());

        EXPECT_EQ(service.balance(client, 1), Money::uah(12'500));
        EXPECT_EQ(service.balance(client, 2), Money::uah(3'750));
    }

    TEST(B1Account, ReadingBalanceMakesNoChanges) {
        TestStorage storage;
        const auto client = CardNumber::parse("4000000000000001");

        storage.seed(Card(client, "Alice", "hash", CardKind::Client));
        storage.seed(Account(1, client, Money::uah(10'000), true));
        storage.seed(Account(2, client, Money::uah(500), false));

        AccountService service(storage);

        (void)service.accountsOf(client);
        EXPECT_EQ(service.balance(client, 1), Money::uah(10'000));
        EXPECT_EQ(service.balance(client, 2), Money::uah(500));

        EXPECT_EQ(storage.accounts().find(1).value().balance(), Money::uah(10'000));
        EXPECT_EQ(storage.accounts().find(2).value().balance(), Money::uah(500));
        EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
    }

    TEST(B1Account, RejectsForeignAndInvalidAccess) {
        TestStorage storage;
        const auto client = CardNumber::parse("4000000000000001");
        const auto otherClient = CardNumber::parse("4000000000000002");
        const auto operatorCard = CardNumber::parse("4000000000000003");
        const auto retainedCard = CardNumber::parse("4000000000000004");
        const auto unknownCard = CardNumber::parse("9000000000009999");

        storage.seed(Card(client, "Alice", "hash", CardKind::Client));
        storage.seed(Card(otherClient, "Bob", "hash", CardKind::Client));
        storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
        storage.seed(Card(retainedCard, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));

        storage.seed(Account(1, client, Money::uah(10'000), true));
        storage.seed(Account(2, otherClient, Money::uah(25'000), true));

        AccountService service(storage);

        EXPECT_DOMAIN_ERROR((void)service.balance(client, 2), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.balance(client, 999), ErrorReason::UnknownAccount);
        EXPECT_DOMAIN_ERROR((void)service.accountsOf(operatorCard), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.accountsOf(retainedCard), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.accountsOf(unknownCard), ErrorReason::UnknownCard);

        EXPECT_EQ(storage.accounts().find(1).value().balance(), Money::uah(10'000));
        EXPECT_EQ(storage.accounts().find(2).value().balance(), Money::uah(25'000));
        EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
    }

}  // namespace
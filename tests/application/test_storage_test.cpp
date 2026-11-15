#include "support/transactional_test_storage.hpp"
#include "support/mini_test.hpp"
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
}

// AtmConfig: default values from the specification and rules they must satisfy.
#include "support/mini_test.hpp"

#include <cstdint>

#include "domain/atm_config.hpp"

namespace {

    using atm::domain::AtmConfig;
    using atm::domain::Money;

    TEST(AtmConfigTest, DefaultsMatchSpecification) {
        const AtmConfig config;
        EXPECT_EQ(config.dailyWithdrawalLimit, Money::uah(10'000));
        EXPECT_EQ(config.journalTailSize, std::size_t{ 20 });
        ASSERT_EQ(config.quickAmounts.size(), std::size_t{ 4 });
        EXPECT_EQ(config.quickAmounts[0], Money::uah(200));
        EXPECT_EQ(config.quickAmounts[1], Money::uah(500));
        EXPECT_EQ(config.quickAmounts[2], Money::uah(1000));
        EXPECT_EQ(config.quickAmounts[3], Money::uah(2000));
    }

    TEST(AtmConfigTest, QuickAmountsArePositiveMultiplesOfHundredWithinDailyLimit) {
        const AtmConfig config;
        for (const Money& amount : config.quickAmounts) {
            EXPECT_TRUE(amount.kopiykas() > 0);
            EXPECT_EQ(amount.kopiykas() % 10'000, 0);  // multiple of 100 UAH (withdrawal rule)
            EXPECT_TRUE(amount <= config.dailyWithdrawalLimit);
        }
    }

    TEST(AtmConfigTest, QuickAmountsAreStrictlyAscending) {
        const AtmConfig config;
        for (std::size_t i = 1; i < config.quickAmounts.size(); ++i) {
            EXPECT_TRUE(config.quickAmounts[i - 1] < config.quickAmounts[i]);
        }
    }

    TEST(AtmConfigTest, CopyIsIndependentOfTheOriginal) {
        const AtmConfig original;
        AtmConfig copy = original;
        copy.journalTailSize = 5;
        copy.quickAmounts.clear();
        copy.dailyWithdrawalLimit = Money::uah(1);
        EXPECT_EQ(original.journalTailSize, std::size_t{ 20 });
        EXPECT_EQ(original.quickAmounts.size(), std::size_t{ 4 });
        EXPECT_EQ(original.dailyWithdrawalLimit, Money::uah(10'000));
    }

}  // namespace
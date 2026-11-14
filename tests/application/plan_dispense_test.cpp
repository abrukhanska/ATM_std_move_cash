// W3: exact cash selection, descending denomination priority, backtracking,
// failure reasons, unchanged inputs, integer boundaries and independent oracles.
#include "support/mini_test.hpp"
#include "support/domain_assertions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <vector>

#include "application/plan_dispense.hpp"
#include "domain/banknote_set.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/money.hpp"

namespace {

    using atm::application::planDispense;
    using atm::domain::BanknoteSet;
    using atm::domain::Denomination;
    using atm::domain::ErrorReason;
    using atm::domain::Money;

    // Every count list below uses this order: 100, 200, 500, 1000, 2000 UAH.
    using Counts = std::array<std::uint32_t, 5>;
    constexpr std::array<Denomination, 5> kDenominations{
        Denomination::UAH100, Denomination::UAH200, Denomination::UAH500,
        Denomination::UAH1000, Denomination::UAH2000
    };

    constexpr std::array<std::int64_t, 5> kUnits{ 1, 2, 5, 10, 20 };

    BanknoteSet makeSet(const Counts& counts) {
        BanknoteSet result;
        for (std::size_t i = 0; i < counts.size(); ++i) {
            result = result.with(kDenominations[i], counts[i]);
        }
        return result;
    }

    bool prefersLargerNotes(const Counts& candidate, const Counts& previous) {
        for (std::size_t end = candidate.size(); end > 0; --end) {
            const auto i = end - 1;
            if (candidate[i] != previous[i]) {
                return candidate[i] > previous[i];
            }
        }
        return false;
    }

    using Reference = std::vector<std::optional<Counts>>;

    void checkPlan(const Money amount, const BanknoteSet& available, const BanknoteSet& expected) {
        const Money amountBefore = amount;
        const BanknoteSet availableBefore = available;
        const auto result = planDispense(amount, available);
        EXPECT_EQ(result, expected);
        EXPECT_EQ(result.total(), amount);
        for (const auto denomination : kDenominations) {
            EXPECT_TRUE(result.count(denomination) <= availableBefore.count(denomination));
        }
        EXPECT_EQ(amount, amountBefore);
        EXPECT_EQ(available, availableBefore);
    }

    void checkFailure(const Money amount, const BanknoteSet& available, ErrorReason reason) {
        const Money amountBefore = amount;
        const BanknoteSet availableBefore = available;
        EXPECT_DOMAIN_ERROR((void)planDispense(amount, available), reason);
        EXPECT_EQ(amount, amountBefore);
        EXPECT_EQ(available, availableBefore);
    }

    void enumerateAllCombinations(
        const Counts& available, std::size_t level, std::int64_t sum,
        Counts& chosen, Reference& reference
    ) {
        if (level == chosen.size()) {
            auto& best = reference[static_cast<std::size_t>(sum)];
            if (!best || prefersLargerNotes(chosen, *best)) {
                best = chosen;
            }
            return;
        }
        for (std::uint32_t count = 0; count <= available[level]; ++count) {
            chosen[level] = count;
            enumerateAllCombinations(available, level + 1,
                sum + static_cast<std::int64_t>(count) * kUnits[level], chosen, reference);
        }
    }

    std::size_t checkAgainstReference(const Counts& counts) {
        std::int64_t capacity = 0;
        for (std::size_t i = 0; i < counts.size(); ++i) {
            capacity += static_cast<std::int64_t>(counts[i]) * kUnits[i];
        }
        Reference reference(static_cast<std::size_t>(capacity + 21));
        Counts chosen{};
        enumerateAllCombinations(counts, 0, 0, chosen, reference);

        const BanknoteSet available = makeSet(counts);
        for (std::int64_t target = 1; target <= capacity + 20; ++target) {
            const auto& expected = reference[static_cast<std::size_t>(target)];
            const Money amount = Money::uah(target * 100);
            if (expected) {
                const auto result = planDispense(amount, available);
                const auto expectedSet = makeSet(*expected);
                if (result != expectedSet) {
                    FAIL() << "inventory=" << available << ", amount=" << amount.format()
                        << ", expected=" << expectedSet << ", actual=" << result;
                }
                EXPECT_EQ(result.total(), amount);
                for (std::size_t i = 0; i < counts.size(); ++i) {
                    EXPECT_TRUE(result.count(kDenominations[i]) <= counts[i]);
                }
            }
            else {
                EXPECT_DOMAIN_ERROR((void)planDispense(amount, available), ErrorReason::AtmOutOfCash);
            }
            EXPECT_EQ(available, makeSet(counts));
        }
        return static_cast<std::size_t>(capacity + 20);
    }

    TEST(PlanDispenseTest, InvalidAmountsAreRejectedBeforeCashAvailability) {
        const BanknoteSet inventories[] = { BanknoteSet{}, makeSet({10, 10, 10, 10, 10}) };
        const std::int64_t invalid[] = {
            0, 1, 99, 100, 9'999, 10'001, 10'050, 15'000,
            (std::numeric_limits<std::int64_t>::max)()
        };
        for (const auto kopiykas : invalid) {
            for (const auto& available : inventories) {
                checkFailure(Money::ofKopiykas(kopiykas), available, ErrorReason::InvalidAmount);
            }
        }
    }

    TEST(PlanDispenseTest, ValuesImmediatelyAroundHundredHryvniaMultiplesAreInvalid) {
        const auto available = makeSet({ 100, 100, 100, 100, 100 });
        for (std::int64_t units = 1; units <= 100; ++units) {
            const std::int64_t kopiykas = units * 10'000;
            checkFailure(Money::ofKopiykas(kopiykas - 1), available, ErrorReason::InvalidAmount);
            checkFailure(Money::ofKopiykas(kopiykas + 1), available, ErrorReason::InvalidAmount);
        }
    }

    TEST(PlanDispenseTest, ValidAmountFromEmptySetIsOutOfCash) {
        EXPECT_DOMAIN_ERROR((void)planDispense(Money::uah(100), BanknoteSet{}),
            ErrorReason::AtmOutOfCash);
    }

    TEST(PlanDispenseTest, EveryDenominationCanBeDispensedOnItsOwn) {
        for (std::size_t i = 0; i < kUnits.size(); ++i) {
            Counts counts{};
            counts[i] = 1;
            const auto available = makeSet(counts);
            checkPlan(Money::uah(kUnits[i] * 100), available, available);
        }
    }

    TEST(PlanDispenseTest, EachDenominationIsPreferredOverAvailableSmallerNotes) {
        const auto available = makeSet({ 100, 100, 100, 100, 100 });
        for (std::size_t i = 0; i < kUnits.size(); ++i) {
            Counts expected{};
            expected[i] = 1;
            checkPlan(Money::uah(kUnits[i] * 100), available, makeSet(expected));
        }
    }

    TEST(PlanDispenseTest, LimitedLargeNotesAreCombinedWithSmallerNotes) {
        const auto available = makeSet({ 3, 1, 1, 1, 1 });
        checkPlan(Money::uah(3'000), available, makeSet({ 0, 0, 0, 1, 1 }));
        checkPlan(Money::uah(2'500), available, makeSet({ 0, 0, 1, 0, 1 }));
        checkPlan(Money::uah(300), available, makeSet({ 1, 1, 0, 0, 0 }));
    }

    TEST(PlanDispenseTest, BacktracksFromFiveHundredToThreeTwoHundreds) {
        const auto available = makeSet({ 0, 3, 1, 0, 0 });
        checkPlan(Money::uah(600), available, makeSet({ 0, 3, 0, 0, 0 }));
    }

    TEST(PlanDispenseTest, PrefersLargerDenominationsWhenBothChoicesWork) {
        const auto available = makeSet({ 10, 10, 10, 10, 10 });
        EXPECT_EQ(planDispense(Money::uah(3'800), available), makeSet({ 1, 1, 1, 1, 1 }));
        EXPECT_EQ(planDispense(Money::uah(600), available), makeSet({ 1, 0, 1, 0, 0 }));
    }

    TEST(PlanDispenseTest, BacktracksAcrossMultipleDenominations) {
        const auto available = makeSet({ 0, 4, 1, 1, 1 });
        checkPlan(Money::uah(2'300), available, makeSet({ 0, 4, 1, 1, 0 }));
    }

    TEST(PlanDispenseTest, ExactCapacityAndOneHundredAboveCapacityAreDistinguished) {
        const auto available = makeSet({ 2, 3, 1, 2, 1 });
        checkPlan(Money::uah(5'300), available, available);
        checkFailure(Money::uah(5'400), available, ErrorReason::AtmOutOfCash);
    }

    TEST(PlanDispenseTest, MissingHundredsRejectOddHundredAmountsWithOnlyTwoHundreds) {
        const auto available = makeSet({ 0, 5, 0, 0, 0 });
        checkFailure(Money::uah(300), available, ErrorReason::AtmOutOfCash);
        checkPlan(Money::uah(400), available, makeSet({ 0, 2, 0, 0, 0 }));
    }

    TEST(PlanDispenseTest, OneHundredNoteDoesNotMakeEveryAmountReachable) {
        const auto available = makeSet({ 1, 0, 1, 1, 0 });
        checkFailure(Money::uah(300), available, ErrorReason::AtmOutOfCash);
        checkPlan(Money::uah(600), available, makeSet({ 1, 0, 1, 0, 0 }));
    }

    TEST(PlanDispenseTest, CapacityConstraintCanRequireAllAvailableLargeNotes) {
        const auto available = makeSet({ 0, 1, 2, 0, 0 });
        checkPlan(Money::uah(1'200), available, available);
    }

    TEST(PlanDispenseTest, UnreachableRemainderIsRejectedAfterBacktracking) {
        const auto available = makeSet({ 0, 4, 1, 0, 0 });
        checkFailure(Money::uah(1'000), available, ErrorReason::AtmOutOfCash);
    }

    TEST(PlanDispenseTest, SuccessAndFailureCallsDoNotShareSearchState) {
        const auto available = makeSet({ 0, 4, 1, 0, 0 });
        checkFailure(Money::uah(1'000), available, ErrorReason::AtmOutOfCash);
        checkPlan(Money::uah(600), available, makeSet({ 0, 3, 0, 0, 0 }));
        checkPlan(Money::uah(900), available, makeSet({ 0, 2, 1, 0, 0 }));
        checkFailure(Money{}, available, ErrorReason::InvalidAmount);
        checkPlan(Money::uah(600), available, makeSet({ 0, 3, 0, 0, 0 }));
    }

    TEST(PlanDispenseTest, ReturnedPlanCanBeChangedWithoutChangingTheInventory) {
        const auto available = makeSet({ 7, 6, 5, 4, 3 });
        const auto before = available;
        auto result = planDispense(Money::uah(4'700), available);
        EXPECT_EQ(result, makeSet({ 0, 1, 1, 0, 2 }));
        EXPECT_EQ(available.minus(result).plus(result), available);
        result = result.with(Denomination::UAH100, 42);
        EXPECT_EQ(available, before);
        EXPECT_EQ(result.count(Denomination::UAH100), std::uint32_t{ 42 });
    }

    TEST(PlanDispenseTest, MillionNoteInventoryProducesTheExactPreferredPlan) {
        const auto available = makeSet({ 1'000'000, 1'000'000, 1'000'000, 1'000'000, 1'000'000 });
        checkPlan(Money::uah(123'400), available, makeSet({ 0, 2, 0, 1, 61 }));
    }

    TEST(PlanDispenseTest, EnoughTotalMoneyDoesNotGuaranteeAnExactCombination) {
        const auto available = makeSet({ 0, 0, 2, 0, 0 });
        EXPECT_DOMAIN_ERROR((void)planDispense(Money::uah(600), available),
            ErrorReason::AtmOutOfCash);
        EXPECT_EQ(available, makeSet({ 0, 0, 2, 0, 0 }));
    }

    TEST(PlanDispenseTest, DailyLimitIsNotPartOfThisFunction) {
        const auto available = makeSet({ 0, 0, 0, 0, 6 });
        EXPECT_EQ(planDispense(Money::uah(12'000), available), available);
    }

    TEST(PlanDispenseTest, MaximumCountsCanAllBeDispensed) {
        constexpr auto maxCount = (std::numeric_limits<std::uint32_t>::max)();
        const auto available = makeSet({ maxCount, maxCount, maxCount, maxCount, maxCount });
        EXPECT_EQ(planDispense(Money::uah(static_cast<std::int64_t>(maxCount) * 3'800), available),
            available);
    }

    TEST(PlanDispenseTest, MaximumCountOfEachSingleDenominationIsSupported) {
        constexpr auto maxCount = (std::numeric_limits<std::uint32_t>::max)();
        for (std::size_t i = 0; i < kUnits.size(); ++i) {
            Counts counts{};
            counts[i] = maxCount;
            const auto available = makeSet(counts);
            const auto hryvnias = static_cast<std::int64_t>(maxCount) * kUnits[i] * 100;
            checkPlan(Money::uah(hryvnias), available, available);
            checkFailure(Money::uah(hryvnias + 100), available, ErrorReason::AtmOutOfCash);
        }
    }

    TEST(PlanDispenseTest, TinyAmountFromMaximumInventoryDoesNotLeaveExtraNotes) {
        constexpr auto maxCount = (std::numeric_limits<std::uint32_t>::max)();
        const auto available = makeSet({ maxCount, maxCount, maxCount, maxCount, maxCount });
        checkPlan(Money::uah(100), available, makeSet({ 1, 0, 0, 0, 0 }));
    }

    TEST(PlanDispenseTest, HugeFeasibleAmountStillBacktracksToAnExactPlan) {
        constexpr auto maxCount = (std::numeric_limits<std::uint32_t>::max)();
        const auto available = makeSet({ 0, maxCount, maxCount, 0, 0 });
        const auto amount = Money::uah(static_cast<std::int64_t>(maxCount) * 500 + 100);
        checkPlan(amount, available, makeSet({ 0, 3, maxCount - 1, 0, 0 }));
    }

    TEST(PlanDispenseTest, HugeImpossibleCombinationWithMaximumCountsIsRejected) {
        constexpr auto maxCount = (std::numeric_limits<std::uint32_t>::max)();
        const auto available = makeSet({ 0, maxCount, 0, 0, maxCount });
        const auto amount = Money::uah(static_cast<std::int64_t>(maxCount) * 2'000 + 100);
        EXPECT_TRUE(available.total() > amount);
        EXPECT_DOMAIN_ERROR((void)planDispense(amount, available), ErrorReason::AtmOutOfCash);
        EXPECT_EQ(available, makeSet({ 0, maxCount, 0, 0, maxCount }));
    }

    TEST(PlanDispenseTest, LargestValidMoneyMultipleExceedsAllCassetteCapacity) {
        constexpr auto maxCount = (std::numeric_limits<std::uint32_t>::max)();
        constexpr auto maxMoney = (std::numeric_limits<std::int64_t>::max)();
        const auto available = makeSet({ maxCount, maxCount, maxCount, maxCount, maxCount });
        EXPECT_DOMAIN_ERROR((void)planDispense(Money::ofKopiykas(maxMoney / 10'000 * 10'000), available),
            ErrorReason::AtmOutOfCash);
    }

    TEST(PlanDispenseTest, HugeValidAmountWithSmallInventoryIsOutOfCash) {
        constexpr auto maxMoney = (std::numeric_limits<std::int64_t>::max)();
        const auto available = makeSet({ 10, 10, 10, 10, 10 });
        checkFailure(Money::ofKopiykas(maxMoney / 10'000 * 10'000), available,
            ErrorReason::AtmOutOfCash);
    }

    TEST(PlanDispenseTest, ExhaustiveCountsZeroThroughThreeMatchIndependentReference) {
        std::size_t checked = 0;
        for (std::uint32_t code = 0; code < 1'024; ++code) {
            Counts counts{};
            auto remaining = code;
            for (auto& count : counts) {
                count = remaining % 4;
                remaining /= 4;
            }
            checked += checkAgainstReference(counts);
        }
        EXPECT_EQ(checked, std::size_t{ 78'848 });
    }

    TEST(PlanDispenseTest, FixedSeedRandomInventoriesMatchIndependentReference) {
        std::mt19937 generator(42);
        std::uniform_int_distribution<std::uint32_t> distribution(0, 12);
        for (int iteration = 0; iteration < 200; ++iteration) {
            Counts counts{};
            for (auto& count : counts) {
                count = distribution(generator);
            }
            (void)checkAgainstReference(counts);
        }
    }

}  // namespace
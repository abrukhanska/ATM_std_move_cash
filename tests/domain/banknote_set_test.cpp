// V2 (BanknoteSet part): counts, with/plus/minus/total that never modify the
// operands, uint32_t overflow, shortage of banknotes, invalid denominations.
#include "support/mini_test.hpp"

#include "support/domain_assertions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>

#include "domain/banknote_set.hpp"
#include "domain/enums.hpp"
#include "domain/money.hpp"

namespace {

    using atm::domain::BanknoteSet;
    using atm::domain::Denomination;
    using atm::domain::ErrorReason;
    using atm::domain::kAllDenominations;
    using atm::domain::Money;

    constexpr std::uint32_t kUint32Max = (std::numeric_limits<std::uint32_t>::max)();

    // --- ST1 ---------------------------------------------------------------------
    static_assert(std::is_trivially_copyable_v<BanknoteSet>);
    static_assert(std::is_nothrow_move_constructible_v<BanknoteSet>);
    static_assert(std::is_nothrow_copy_constructible_v<BanknoteSet>);
    static_assert(std::is_nothrow_default_constructible_v<BanknoteSet>);
    // Equality of empty sets must be usable in constant expressions.
    static_assert(BanknoteSet{} == BanknoteSet{});

    BanknoteSet makeSet(std::uint32_t n100, std::uint32_t n200, std::uint32_t n500,
        std::uint32_t n1000, std::uint32_t n2000) {
        return BanknoteSet{}
            .with(Denomination::UAH100, n100)
            .with(Denomination::UAH200, n200)
            .with(Denomination::UAH500, n500)
            .with(Denomination::UAH1000, n1000)
            .with(Denomination::UAH2000, n2000);
    }

    // --- construction, with -----------------------------------------------------------

    TEST(V2BanknoteSet, DefaultSetIsEmptyWithZeroTotal) {
        const BanknoteSet set;
        EXPECT_EQ(set, BanknoteSet{});
        EXPECT_EQ(set.total().kopiykas(), 0);
        for (const auto d : kAllDenominations) {
            EXPECT_EQ(set.count(d), 0U);
        }
    }

    TEST(V2BanknoteSet, WithReplacesCountAndKeepsOriginal) {
        const BanknoteSet original;
        const BanknoteSet changed = original.with(Denomination::UAH500, 3);
        EXPECT_EQ(changed.count(Denomination::UAH500), 3U);
        EXPECT_EQ(original.count(Denomination::UAH500), 0U);
        EXPECT_NE(changed, original);
    }

    TEST(V2BanknoteSet, WithReplacesRatherThanAdds) {
        const BanknoteSet set =
            BanknoteSet{}.with(Denomination::UAH100, 5).with(Denomination::UAH100, 2);
        EXPECT_EQ(set.count(Denomination::UAH100), 2U);
    }

    TEST(V2BanknoteSet, WithTouchesOnlyRequestedDenomination) {
        const BanknoteSet set = makeSet(1, 2, 3, 4, 5).with(Denomination::UAH500, 9);
        EXPECT_EQ(set, makeSet(1, 2, 9, 4, 5));
    }

    TEST(V2BanknoteSet, WithChangesExactlyOneDenominationForEveryDenomination) {
        const BanknoteSet base = makeSet(1, 2, 3, 4, 5);
        for (const auto changed : kAllDenominations) {
            const BanknoteSet result = base.with(changed, 77);
            for (const auto d : kAllDenominations) {
                EXPECT_EQ(result.count(d), d == changed ? 77U : base.count(d));
            }
        }
    }

    TEST(V2BanknoteSet, WithZeroClearsDenomination) {
        EXPECT_EQ(makeSet(1, 2, 3, 4, 5).with(Denomination::UAH1000, 0), makeSet(1, 2, 3, 0, 5));
    }

    // --- total ------------------------------------------------------------------------

    TEST(V2BanknoteSet, TotalSumsAllDenominations) {
        // 1*100 + 2*200 + 3*500 + 4*1000 + 5*2000 = 100 + 400 + 1500 + 4000 + 10000
        EXPECT_EQ(makeSet(1, 2, 3, 4, 5).total(), Money::uah(16'000));
    }

    TEST(V2BanknoteSet, TotalOfEachSingleDenomination) {
        constexpr std::int64_t expectedUah[] = { 100, 200, 500, 1000, 2000 };
        for (std::size_t i = 0; i < atm::domain::kDenominationCount; ++i) {
            const BanknoteSet set = BanknoteSet{}.with(kAllDenominations[i], 1);
            EXPECT_EQ(set.total(), Money::uah(expectedUah[i]));
        }
    }

    TEST(V2BanknoteSet, TotalMatchesSpecificationExample) {
        // The planDispense example: one 500 and three 200 banknotes hold 1100 UAH.
        EXPECT_EQ(makeSet(0, 3, 1, 0, 0).total(), Money::uah(1'100));
    }

    TEST(V2BanknoteSet, TotalWithMaxCountDoesNotOverflowUint32Math) {
        // 4 294 967 295 * 2000 = 8 589 934 590 000 UAH: overflows uint32_t, fits in Money.
        const BanknoteSet set = BanknoteSet{}.with(Denomination::UAH2000, kUint32Max);
        EXPECT_EQ(set.total(), Money::uah(8'589'934'590'000));
    }

    TEST(V2BanknoteSet, TotalOfAllMaxCountsIsExact) {
        // 4 294 967 295 * (100 + 200 + 500 + 1000 + 2000) = 16 320 875 721 000 UAH.
        BanknoteSet set;
        for (const auto d : kAllDenominations) {
            set = set.with(d, kUint32Max);
        }
        EXPECT_EQ(set.total(), Money::uah(16'320'875'721'000));
    }

    // --- plus ------------------------------------------------------------------------

    TEST(V2BanknoteSet, PlusAddsPerDenominationAndKeepsOperands) {
        const BanknoteSet a = makeSet(1, 0, 2, 0, 0);
        const BanknoteSet b = makeSet(4, 1, 0, 0, 3);
        const BanknoteSet sum = a.plus(b);
        EXPECT_EQ(sum, makeSet(5, 1, 2, 0, 3));
        EXPECT_EQ(a, makeSet(1, 0, 2, 0, 0));
        EXPECT_EQ(b, makeSet(4, 1, 0, 0, 3));
    }

    TEST(V2BanknoteSet, PlusWithEmptySetIsIdentity) {
        const BanknoteSet a = makeSet(1, 2, 3, 4, 5);
        EXPECT_EQ(a.plus(BanknoteSet{}), a);
        EXPECT_EQ(BanknoteSet{}.plus(a), a);
    }

    TEST(V2BanknoteSet, PlusWithItselfDoublesCounts) {
        const BanknoteSet a = makeSet(1, 2, 3, 4, 5);
        EXPECT_EQ(a.plus(a), makeSet(2, 4, 6, 8, 10));
        EXPECT_EQ(a, makeSet(1, 2, 3, 4, 5));
    }

    TEST(V2BanknoteSet, PlusAtExactUint32LimitIsAllowed) {
        for (const auto d : kAllDenominations) {
            const BanknoteSet a = BanknoteSet{}.with(d, kUint32Max - 1);
            const BanknoteSet b = BanknoteSet{}.with(d, 1);
            EXPECT_EQ(a.plus(b).count(d), kUint32Max);
        }
    }

    TEST(V2BanknoteSet, PlusOverflowInAnyDenominationThrowsAndKeepsOperands) {
        for (const auto d : kAllDenominations) {
            const BanknoteSet full = BanknoteSet{}.with(d, kUint32Max);
            const BanknoteSet one = BanknoteSet{}.with(d, 1);
            EXPECT_DOMAIN_ERROR((void)full.plus(one), ErrorReason::Overflow);
            EXPECT_DOMAIN_ERROR((void)one.plus(full), ErrorReason::Overflow);
            EXPECT_EQ(full.count(d), kUint32Max);
            EXPECT_EQ(one.count(d), 1U);
        }
    }

    TEST(V2BanknoteSet, PlusOverflowDoesNotDependOnOtherDenominations) {
        const BanknoteSet a = BanknoteSet{}.with(Denomination::UAH1000, kUint32Max);
        const BanknoteSet b =
            BanknoteSet{}.with(Denomination::UAH100, 5).with(Denomination::UAH1000, 1);
        EXPECT_DOMAIN_ERROR((void)a.plus(b), ErrorReason::Overflow);
        EXPECT_EQ(b.count(Denomination::UAH100), 5U);
    }

    // --- minus -----------------------------------------------------------------------

    TEST(V2BanknoteSet, MinusSubtractsPerDenominationAndKeepsOperands) {
        const BanknoteSet a = makeSet(5, 4, 3, 2, 1);
        const BanknoteSet b = makeSet(1, 1, 1, 1, 1);
        EXPECT_EQ(a.minus(b), makeSet(4, 3, 2, 1, 0));
        EXPECT_EQ(a, makeSet(5, 4, 3, 2, 1));
        EXPECT_EQ(b, makeSet(1, 1, 1, 1, 1));
    }

    TEST(V2BanknoteSet, MinusExactlyAllGivesEmptySet) {
        const BanknoteSet a = makeSet(1, 2, 3, 4, 5);
        EXPECT_EQ(a.minus(a), BanknoteSet{});
    }

    TEST(V2BanknoteSet, MinusEmptySetIsIdentity) {
        const BanknoteSet a = makeSet(1, 2, 3, 4, 5);
        EXPECT_EQ(a.minus(BanknoteSet{}), a);
    }

    TEST(V2BanknoteSet, MinusShortageInAnyDenominationThrowsInvalidAmount) {
        const BanknoteSet stock = makeSet(5, 5, 5, 5, 5);
        for (const auto d : kAllDenominations) {
            const BanknoteSet request = BanknoteSet{}.with(d, 6);
            EXPECT_DOMAIN_ERROR((void)stock.minus(request), ErrorReason::InvalidAmount);
            EXPECT_EQ(stock, makeSet(5, 5, 5, 5, 5));
        }
    }

    TEST(V2BanknoteSet, MinusFromEmptyThrows) {
        const BanknoteSet oneHundred = BanknoteSet{}.with(Denomination::UAH100, 1);
        EXPECT_DOMAIN_ERROR((void)BanknoteSet {}.minus(oneHundred), ErrorReason::InvalidAmount);
    }

    TEST(V2BanknoteSet, PlusThenMinusRestoresOriginal) {
        const BanknoteSet a = makeSet(1, 2, 3, 4, 5);
        const BanknoteSet b = makeSet(9, 8, 7, 6, 5);
        EXPECT_EQ(a.plus(b).minus(b), a);
        EXPECT_EQ(a.plus(b).minus(a), b);
    }

    // --- invalid denominations ----------------------------------------------------------

    TEST(V2BanknoteSet, InvalidDenominationIsRejectedWithInvalidData) {
        const Denomination invalid[] = { static_cast<Denomination>(5),
                                        static_cast<Denomination>(99),
                                        static_cast<Denomination>(-1) };
        const BanknoteSet set = makeSet(1, 2, 3, 4, 5);
        for (const auto bad : invalid) {
            EXPECT_DOMAIN_ERROR((void)set.count(bad), ErrorReason::InvalidData);
            EXPECT_DOMAIN_ERROR((void)set.with(bad, 1), ErrorReason::InvalidData);
        }
        EXPECT_EQ(set, makeSet(1, 2, 3, 4, 5));
    }

    // --- equality ---------------------------------------------------------------------

    TEST(V2BanknoteSet, EqualityComparesAllCounts) {
        const BanknoteSet base = makeSet(1, 2, 3, 4, 5);
        EXPECT_EQ(base, makeSet(1, 2, 3, 4, 5));
        for (const auto d : kAllDenominations) {
            EXPECT_NE(base, base.with(d, 99));            // differs in exactly one denomination
            EXPECT_EQ(base.with(d, base.count(d)), base);  // same value written back
        }
    }

    TEST(V2BanknoteSet, EqualTotalsDoNotMeanEqualBanknoteSets) {
        const BanknoteSet hundreds =
            BanknoteSet{}.with(Denomination::UAH100, 5);

        const BanknoteSet fiveHundred =
            BanknoteSet{}.with(Denomination::UAH500, 1);

        EXPECT_EQ(hundreds.total(), fiveHundred.total());
        EXPECT_NE(hundreds, fiveHundred);

        EXPECT_DOMAIN_ERROR(
            (void)hundreds.minus(fiveHundred),
            ErrorReason::InvalidAmount
        );
    }

    // --- randomized checks against an independent oracle (fixed seed: reproducible) ---------

    BanknoteSet fromCounts(const std::array<std::uint32_t, 5>& counts) {
        BanknoteSet set;
        for (std::size_t i = 0; i < counts.size(); ++i) {
            set = set.with(kAllDenominations[i], counts[i]);
        }
        return set;
    }

    TEST(V2BanknoteSet, PlusMinusTotalMatchOracleOnRandomSets) {
        // Values written out here on purpose: the oracle must not reuse denominationUah().
        constexpr std::int64_t kValues[5] = { 100, 200, 500, 1000, 2000 };
        std::mt19937_64 rng(20260201);
        std::uniform_int_distribution<std::uint32_t> countDist(0, 1'000'000);

        for (int round = 0; round < 2'000; ++round) {
            std::array<std::uint32_t, 5> a{};
            std::array<std::uint32_t, 5> b{};
            std::array<std::uint32_t, 5> half{};
            std::int64_t totalA = 0;
            std::int64_t totalB = 0;
            std::int64_t totalHalf = 0;
            bool enough = true;
            for (std::size_t k = 0; k < 5; ++k) {
                a[k] = countDist(rng);
                b[k] = countDist(rng);
                half[k] = a[k] / 2;
                totalA += static_cast<std::int64_t>(a[k]) * kValues[k];
                totalB += static_cast<std::int64_t>(b[k]) * kValues[k];
                totalHalf += static_cast<std::int64_t>(half[k]) * kValues[k];
                if (a[k] < b[k]) {
                    enough = false;
                }
            }
            const BanknoteSet setA = fromCounts(a);
            const BanknoteSet setB = fromCounts(b);
            const BanknoteSet setHalf = fromCounts(half);

            EXPECT_EQ(setA.total(), Money::uah(totalA));
            EXPECT_EQ(setA.plus(setB).total(), Money::uah(totalA + totalB));
            EXPECT_EQ(setA.plus(setB), setB.plus(setA));
            EXPECT_EQ(setA.plus(setB).minus(setB), setA);

            // Always enough: half of every count.
            EXPECT_EQ(setA.minus(setHalf).total(), Money::uah(totalA - totalHalf));

            if (enough) {
                EXPECT_EQ(setA.minus(setB).total(), Money::uah(totalA - totalB));
            }
            else {
                EXPECT_DOMAIN_ERROR((void)setA.minus(setB), ErrorReason::InvalidAmount);
            }
        }
    }

}  // namespace
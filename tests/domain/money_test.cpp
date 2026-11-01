// V1 - Money: hryvnias to kopiykas, zero, negative values, overflow limits,
// arithmetic that never modifies its operands, formatting.
#include "support/mini_test.hpp"

#include "support/domain_assertions.hpp"

#include <compare>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <utility>

#include "domain/money.hpp"

namespace {

    using atm::domain::DomainError;
    using atm::domain::ErrorReason;
    using atm::domain::Money;

    constexpr std::int64_t kInt64Max = (std::numeric_limits<std::int64_t>::max)();

    // --- ST1: type properties ---------------------------------------------------
    static_assert(noexcept(std::declval<const Money&>().kopiykas()));
    static_assert(noexcept(std::declval<const Money&>() == std::declval<const Money&>()));
    static_assert(std::is_trivially_copyable_v<Money>);
    static_assert(std::is_nothrow_move_constructible_v<Money>);
    static_assert(std::is_nothrow_default_constructible_v<Money>);
    // No public way to build an unchecked value.
    static_assert(!std::is_convertible_v<std::int64_t, Money>);
    static_assert(!std::is_constructible_v<Money, std::int64_t>);
    // Ordering category promised by the defaulted <=> on a single int64_t.
    static_assert(std::is_same_v<decltype(Money{} <=> Money{}), std::strong_ordering > );

    // --- constexpr evaluation (throw branches are not taken) -------------------------
    static_assert(Money::uah(150).kopiykas() == 15'000);
    static_assert(Money::uah(1) + Money::ofKopiykas(50) == Money::ofKopiykas(150));
    static_assert(Money::uah(2) - Money::uah(1) == Money::uah(1));
    static_assert(Money::uah(1) < Money::uah(2));
    static_assert(Money{} == Money::uah(0));

    // Independent oracle for format(): digits of the kopiykas with a dot before the last two.
    std::string expectedFormat(std::int64_t kopiykas) {
        std::string digits = std::to_string(kopiykas);
        if (digits.size() < 3) {
            digits.insert(0, 3 - digits.size(), '0');
        }
        digits.insert(digits.size() - 2, ".");
        return digits;
    }

    // --- uah ------------------------------------------------------------------------

    TEST(V1Money, UahConvertsWholeHryvniasToKopiykas) {
        EXPECT_EQ(Money::uah(1).kopiykas(), 100);
        EXPECT_EQ(Money::uah(100).kopiykas(), 10'000);
        EXPECT_EQ(Money::uah(10'000).kopiykas(), 1'000'000);  // daily limit from the specification
    }

    TEST(V1Money, UahZeroIsAllowedAndEqualsDefault) {
        EXPECT_EQ(Money::uah(0).kopiykas(), 0);
        EXPECT_EQ(Money::uah(0), Money{});
        EXPECT_EQ(Money::uah(0), Money::ofKopiykas(0));
    }

    TEST(V1Money, UahRejectsNegative) {
        EXPECT_DOMAIN_ERROR((void)Money::uah(-1), ErrorReason::InvalidAmount);
        EXPECT_DOMAIN_ERROR((void)Money::uah((std::numeric_limits<std::int64_t>::min)()),
            ErrorReason::InvalidAmount);
    }

    TEST(V1Money, UahAcceptsExactUpperBound) {
        // INT64_MAX / 100 = 92 233 720 368 547 758; times 100 = 9 223 372 036 854 775 800.
        EXPECT_EQ(Money::uah(92'233'720'368'547'758).kopiykas(), 9'223'372'036'854'775'800);
        EXPECT_EQ(Money::uah(kInt64Max / 100).kopiykas(), 9'223'372'036'854'775'800);
    }

    TEST(V1Money, UahRejectsOverflowByOne) {
        EXPECT_DOMAIN_ERROR((void)Money::uah(92'233'720'368'547'759), ErrorReason::Overflow);
        EXPECT_DOMAIN_ERROR((void)Money::uah(kInt64Max / 100 + 1), ErrorReason::Overflow);
        EXPECT_DOMAIN_ERROR((void)Money::uah(kInt64Max), ErrorReason::Overflow);
    }

    // --- ofKopiykas -----------------------------------------------------------------

    TEST(V1Money, OfKopiykasAcceptsNonNegative) {
        EXPECT_EQ(Money::ofKopiykas(0).kopiykas(), 0);
        EXPECT_EQ(Money::ofKopiykas(1).kopiykas(), 1);
        EXPECT_EQ(Money::ofKopiykas(kInt64Max).kopiykas(), kInt64Max);
    }

    TEST(V1Money, OfKopiykasRejectsNegative) {
        EXPECT_DOMAIN_ERROR((void)Money::ofKopiykas(-1), ErrorReason::InvalidAmount);
        EXPECT_DOMAIN_ERROR((void)Money::ofKopiykas((std::numeric_limits<std::int64_t>::min)()),
            ErrorReason::InvalidAmount);
    }

    TEST(V1Money, DefaultConstructedIsZero) {
        EXPECT_EQ(Money{}.kopiykas(), 0);
        EXPECT_EQ(Money{}, Money{});
    }

    // --- addition -------------------------------------------------------------------

    TEST(V1Money, AdditionSumsKopiykas) {
        EXPECT_EQ((Money::uah(100) + Money::ofKopiykas(50)).kopiykas(), 10'050);
        EXPECT_EQ((Money{} + Money{}).kopiykas(), 0);
    }

    TEST(V1Money, AdditionAtExactLimitIsAllowed) {
        const Money almostMax = Money::ofKopiykas(kInt64Max - 1);
        EXPECT_EQ((almostMax + Money::ofKopiykas(1)).kopiykas(), kInt64Max);
        EXPECT_EQ((Money::ofKopiykas(kInt64Max) + Money{}).kopiykas(), kInt64Max);
    }

    TEST(V1Money, AdditionOverflowByOneKopiykaThrows) {
        const Money max = Money::ofKopiykas(kInt64Max);
        EXPECT_DOMAIN_ERROR((void)(max + Money::ofKopiykas(1)), ErrorReason::Overflow);
        EXPECT_DOMAIN_ERROR((void)(Money::ofKopiykas(1) + max), ErrorReason::Overflow);
        EXPECT_DOMAIN_ERROR((void)(max + max), ErrorReason::Overflow);
    }

    // --- subtraction ----------------------------------------------------------------

    TEST(V1Money, SubtractionGivesDifference) {
        EXPECT_EQ((Money::uah(100) - Money::uah(40)).kopiykas(), 6'000);
    }

    TEST(V1Money, SubtractionOfEqualValuesGivesZero) {
        EXPECT_EQ(Money::uah(5) - Money::uah(5), Money{});
        EXPECT_EQ(Money::ofKopiykas(kInt64Max) - Money::ofKopiykas(kInt64Max), Money{});
    }

    TEST(V1Money, SubtractionBelowZeroThrowsInvalidAmount) {
        EXPECT_DOMAIN_ERROR((void)(Money::ofKopiykas(99) - Money::uah(1)),
            ErrorReason::InvalidAmount);
        EXPECT_DOMAIN_ERROR((void)(Money{} - Money::ofKopiykas(1)), ErrorReason::InvalidAmount);
    }

    TEST(V1Money, ArithmeticDoesNotModifyOperands) {
        const Money a = Money::uah(10);
        const Money b = Money::uah(3);
        (void)(a + b);
        (void)(a - b);
        EXPECT_EQ(a.kopiykas(), 1'000);
        EXPECT_EQ(b.kopiykas(), 300);
    }

    TEST(V1Money, FailedArithmeticLeavesOperandsUntouched) {
        const Money smaller = Money::uah(1);
        const Money big = Money::uah(2);
        EXPECT_THROW((void)(smaller - big), DomainError);
        EXPECT_EQ(smaller.kopiykas(), 100);
        EXPECT_EQ(big.kopiykas(), 200);

        const Money max = Money::ofKopiykas(kInt64Max);
        EXPECT_THROW((void)(max + big), DomainError);
        EXPECT_EQ(max.kopiykas(), kInt64Max);
        EXPECT_EQ(big.kopiykas(), 200);
    }

    // --- comparison: both true and false outcomes ------------------------------------

    TEST(V1Money, ComparisonOperatorsTrueCases) {
        EXPECT_TRUE(Money::uah(1) < Money::uah(2));
        EXPECT_TRUE(Money::uah(2) > Money::ofKopiykas(199));
        EXPECT_TRUE(Money::uah(1) <= Money::ofKopiykas(100));
        EXPECT_TRUE(Money::uah(1) <= Money::uah(2));
        EXPECT_TRUE(Money::uah(1) >= Money::ofKopiykas(100));
        EXPECT_TRUE(Money::uah(2) >= Money::uah(1));
        EXPECT_TRUE(Money::uah(1) == Money::ofKopiykas(100));
        EXPECT_TRUE(Money::uah(1) != Money::uah(2));
    }

    TEST(V1Money, ComparisonOperatorsFalseCases) {
        EXPECT_FALSE(Money::uah(2) < Money::uah(1));
        EXPECT_FALSE(Money::uah(1) < Money::uah(1));
        EXPECT_FALSE(Money::uah(1) > Money::uah(1));
        EXPECT_FALSE(Money::uah(1) > Money::uah(2));
        EXPECT_FALSE(Money::uah(2) <= Money::uah(1));
        EXPECT_FALSE(Money::uah(1) >= Money::uah(2));
        EXPECT_FALSE(Money::uah(1) == Money::uah(2));
        EXPECT_FALSE(Money::uah(1) != Money::uah(1));
    }

    TEST(V1Money, AddThenSubtractRestoresOriginal) {
        const Money a = Money::uah(123);
        const Money b = Money::ofKopiykas(45);
        EXPECT_EQ((a + b) - b, a);
        EXPECT_EQ((a + b) - a, b);
        EXPECT_EQ(a + b, b + a);
    }

    TEST(V1Money, OneKopiykaDifferenceIsDetected) {
        EXPECT_TRUE(Money::ofKopiykas(99) < Money::uah(1));
        EXPECT_TRUE(Money::ofKopiykas(101) > Money::uah(1));
    }

    // --- format ---------------------------------------------------------------------

    TEST(V1Money, FormatAlwaysShowsTwoDigitsOfKopiykas) {
        EXPECT_EQ(Money{}.format(), "0.00");
        EXPECT_EQ(Money::ofKopiykas(1).format(), "0.01");
        EXPECT_EQ(Money::ofKopiykas(10).format(), "0.10");
        EXPECT_EQ(Money::ofKopiykas(99).format(), "0.99");
        EXPECT_EQ(Money::ofKopiykas(100).format(), "1.00");
        EXPECT_EQ(Money::ofKopiykas(9'999).format(), "99.99");
        EXPECT_EQ(Money::uah(100).format(), "100.00");
        EXPECT_EQ(Money::ofKopiykas(10'050).format(), "100.50");
        EXPECT_EQ(Money::ofKopiykas(10'005).format(), "100.05");
        EXPECT_EQ(Money::uah(10'000).format(), "10000.00");
    }

    TEST(V1Money, FormatHandlesLargestValue) {
        EXPECT_EQ(Money::ofKopiykas(kInt64Max).format(), "92233720368547758.07");
    }

    // --- randomized checks against independent oracles (fixed seed: reproducible) ------

    TEST(V1Money, UahMatchesOracleOnRandomValues) {
        std::mt19937_64 rng(20260101);
        std::uniform_int_distribution<std::int64_t> dist(0, kInt64Max / 100);
        for (int i = 0; i < 10'000; ++i) {
            const std::int64_t hryvnias = dist(rng);
            const std::int64_t kopiykas = Money::uah(hryvnias).kopiykas();
            EXPECT_EQ(kopiykas / 100, hryvnias);
            EXPECT_EQ(kopiykas % 100, 0);
        }
    }

    TEST(V1Money, AdditionMatchesUnsignedOracleOnRandomValues) {
        std::mt19937_64 rng(20260102);
        std::uniform_int_distribution<std::int64_t> dist(0, kInt64Max);
        for (int i = 0; i < 10'000; ++i) {
            const std::int64_t a = dist(rng);
            const std::int64_t b = dist(rng);
            // Unsigned addition cannot overflow here: both operands are below 2^63.
            const std::uint64_t sum = static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b);
            if (sum > static_cast<std::uint64_t>(kInt64Max)) {
                EXPECT_DOMAIN_ERROR((void)(Money::ofKopiykas(a) + Money::ofKopiykas(b)),
                    ErrorReason::Overflow);
            }
            else {
                EXPECT_EQ((Money::ofKopiykas(a) + Money::ofKopiykas(b)).kopiykas(),
                    static_cast<std::int64_t>(sum));
            }
        }
    }

    TEST(V1Money, SubtractionMatchesOracleOnRandomValues) {
        std::mt19937_64 rng(20260103);
        std::uniform_int_distribution<std::int64_t> dist(0, kInt64Max);
        for (int i = 0; i < 10'000; ++i) {
            const std::int64_t a = dist(rng);
            const std::int64_t b = dist(rng);
            if (a < b) {
                EXPECT_DOMAIN_ERROR((void)(Money::ofKopiykas(a) - Money::ofKopiykas(b)),
                    ErrorReason::InvalidAmount);
            }
            else {
                EXPECT_EQ((Money::ofKopiykas(a) - Money::ofKopiykas(b)).kopiykas(), a - b);
            }
        }
    }

    TEST(V1Money, ComparisonMatchesKopiykasOrderOnRandomValues) {
        std::mt19937_64 rng(20260105);
        std::uniform_int_distribution<std::int64_t> dist(0, kInt64Max);
        for (int i = 0; i < 10'000; ++i) {
            const std::int64_t a = dist(rng);
            const std::int64_t b = (i % 10 == 0) ? a : dist(rng);  // also equal pairs
            const Money ma = Money::ofKopiykas(a);
            const Money mb = Money::ofKopiykas(b);
            EXPECT_EQ(ma < mb, a < b);
            EXPECT_EQ(ma > mb, a > b);
            EXPECT_EQ(ma <= mb, a <= b);
            EXPECT_EQ(ma >= mb, a >= b);
            EXPECT_EQ(ma == mb, a == b);
            EXPECT_EQ(ma != mb, a != b);
        }
    }

    TEST(V1Money, FormatMatchesOracleOnRandomValues) {
        std::mt19937_64 rng(20260104);
        std::uniform_int_distribution<std::int64_t> wide(0, kInt64Max);
        std::uniform_int_distribution<std::int64_t> lowRange(0, 100'000);
        for (int i = 0; i < 5'000; ++i) {
            const std::int64_t w = wide(rng);
            const std::int64_t s = lowRange(rng);
            EXPECT_EQ(Money::ofKopiykas(w).format(), expectedFormat(w));
            EXPECT_EQ(Money::ofKopiykas(s).format(), expectedFormat(s));
        }
    }

}  // namespace
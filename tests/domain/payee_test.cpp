// Payee: plain record from the in-code directory. The directory rules
// (positive unique ids, valid categories) are checked by PaymentService.
#include "support/mini_test.hpp"

#include <string>
#include <type_traits>

#include "domain/enums.hpp"
#include "domain/payee.hpp"

namespace {

    using atm::domain::Payee;
    using atm::domain::PayeeCategory;

    static_assert(std::is_aggregate_v<Payee>);
    static_assert(std::is_nothrow_move_constructible_v<Payee>);

    TEST(PayeeTest, DefaultValues) {
        const Payee payee;
        EXPECT_EQ(payee.id, 0);
        EXPECT_TRUE(payee.name.empty());
        EXPECT_EQ(payee.category, PayeeCategory::MobileOperator);
    }

    TEST(PayeeTest, AggregateInitialization) {
        const Payee payee{ 2, "Charity", PayeeCategory::Charity };
        EXPECT_EQ(payee.id, 2);
        EXPECT_EQ(payee.name, "Charity");
        EXPECT_EQ(payee.category, PayeeCategory::Charity);
    }

    TEST(PayeeTest, CopyIsIndependent) {
        const Payee original{ 1, "Mobile", PayeeCategory::MobileOperator };
        Payee copy = original;
        copy.name = "Changed";
        copy.id = 7;
        EXPECT_EQ(original.name, "Mobile");
        EXPECT_EQ(original.id, 1);
    }

}  // namespace
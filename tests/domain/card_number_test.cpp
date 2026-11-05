#include "support/mini_test.hpp"
#include "domain/card_number.hpp"

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using atm::domain::CardNumber;

static_assert(noexcept(std::declval<const CardNumber&>() == std::declval<const CardNumber&>()));
static_assert(std::is_trivially_copyable_v<CardNumber>);
static_assert(std::is_nothrow_move_constructible_v<CardNumber>);
static_assert(std::is_nothrow_default_constructible_v<CardNumber>);

static_assert(!std::is_convertible_v<std::string, CardNumber>);

// --- str & masked & format & parse ---

TEST(V2CardNumber, Str_Masked_Format_DefaultZeroes) {
    const CardNumber card{};
    EXPECT_EQ(card.str(), "0000000000000000");
    EXPECT_EQ(card.masked(), "**** **** **** 0000");
    EXPECT_EQ(card.format(), "0000 0000 0000 0000");
}

TEST(V2CardNumber, ParseAcceptsValidSixteenDigits) {
    const auto card = CardNumber::parse("1234567890123456");
    EXPECT_EQ(card.str(), "1234567890123456");
}

TEST(V2CardNumber, ParsePreservesLeadingZeros) {
    const auto card = CardNumber::parse("0000123456789012");
    EXPECT_EQ(card.str(), "0000123456789012");
}

TEST(V2CardNumber, Parse_ThrowsInvalidArgument_InvalidLength) {
    EXPECT_THROW((void)CardNumber::parse("123456789012345"), std::invalid_argument);
    EXPECT_THROW((void)CardNumber::parse("12345678901234567"), std::invalid_argument);
    EXPECT_THROW((void)CardNumber::parse(""), std::invalid_argument);
}

TEST(V2CardNumber, Parse_ThrowsInvalidArgument_InvalidCharacters) {
    EXPECT_THROW((void)CardNumber::parse("123456789012345a"), std::invalid_argument);
    EXPECT_THROW((void)CardNumber::parse("1234 5678 9012 34"), std::invalid_argument);
    EXPECT_THROW((void)CardNumber::parse("1234-5678-9012-3456"), std::invalid_argument);
}

TEST(V2CardNumber, Str_Masked_Format_ValidCard) {
    const auto card = CardNumber::parse("4000123456789010");
    EXPECT_EQ(card.str(), "4000123456789010");
    EXPECT_EQ(card.masked(), "**** **** **** 9010");
    EXPECT_EQ(card.format(), "4000 1234 5678 9010");
}

TEST(V2CardNumber, ComparisonOperators) {
    const auto card1 = CardNumber::parse("4000123456789010");
    const auto card2 = CardNumber::parse("4000123456789010");
    const auto card3 = CardNumber::parse("4000123456789011");

    EXPECT_TRUE(card1 == card2);
    EXPECT_FALSE(card1 == card3);

    EXPECT_FALSE(card1 != card2);
    EXPECT_TRUE(card1 != card3);
}

} // namespace
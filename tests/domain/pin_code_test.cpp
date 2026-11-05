#include "support/mini_test.hpp"
#include "domain/pin_code.hpp"

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using atm::domain::PinCode;

static_assert(noexcept(std::declval<const PinCode&>().digits()));
static_assert(noexcept(std::declval<const PinCode&>() == std::declval<const PinCode&>()));

static_assert(std::is_trivially_copyable_v<PinCode>);
static_assert(std::is_nothrow_move_constructible_v<PinCode>);
static_assert(std::is_nothrow_default_constructible_v<PinCode>);

static_assert(!std::is_convertible_v<std::string, PinCode>);

// --- digits & str ---

TEST(V2PinCode, Str_Digits_DefaultZeroes) {
    const PinCode pin{};
    EXPECT_EQ(pin.str(), "0000");
    
    const std::array<char, 4> expected{'0', '0', '0', '0'};
    EXPECT_EQ(pin.digits(), expected);
}

TEST(V2PinCode, Str_Digits_ValidPin) {
    const auto pin = PinCode::parse("1234");
    EXPECT_EQ(pin.str(), "1234");
    
    const std::array<char, 4> expected{'1', '2', '3', '4'};
    EXPECT_EQ(pin.digits(), expected);
}

// --- parse ---

TEST(V2PinCode, Parse_ValidPin) {
    const auto pin = PinCode::parse("0007");
    EXPECT_EQ(pin.str(), "0007");
    EXPECT_EQ(pin.digits()[0], '0');
    EXPECT_EQ(pin.digits()[3], '7');
}

TEST(V2PinCode, Parse_ThrowsInvalidArgument_InvalidLength) {
    EXPECT_THROW((void)PinCode::parse("123"), std::invalid_argument);
    EXPECT_THROW((void)PinCode::parse("12345"), std::invalid_argument);
    EXPECT_THROW((void)PinCode::parse(""), std::invalid_argument);
}

TEST(V2PinCode, Parse_ThrowsInvalidArgument_InvalidCharacters) {
    EXPECT_THROW((void)PinCode::parse("12a4"), std::invalid_argument);
    // Пробіли
    EXPECT_THROW((void)PinCode::parse("12 4"), std::invalid_argument);
    // Спецсимволи
    EXPECT_THROW((void)PinCode::parse("12-4"), std::invalid_argument);
}

TEST(V2PinCode, EqualityOperators) {
    const auto pin1 = PinCode::parse("1234");
    const auto pin2 = PinCode::parse("1234");
    const auto pin3 = PinCode::parse("4321");

    EXPECT_TRUE(pin1 == pin2);
    EXPECT_FALSE(pin1 == pin3);

    EXPECT_FALSE(pin1 != pin2);
    EXPECT_TRUE(pin1 != pin3);
}

} // namespace
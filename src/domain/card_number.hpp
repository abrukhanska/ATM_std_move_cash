#ifndef ATM_DOMAIN_CARD_NUMBER_HPP
#define ATM_DOMAIN_CARD_NUMBER_HPP

#include <array>
#include <string>
#include <string_view>

namespace atm::domain {

    // A class representing a bank card number.
    class CardNumber {
    public:
        // Creates a default card number containing all zero digits.
        constexpr CardNumber() noexcept { digits_.fill('0'); }

        // Creates a card number from its string representation.
        [[nodiscard]] static CardNumber parse(std::string_view str);

        // Returns the card number as a 16-digit string.
        [[nodiscard]] std::string str() const {
            return std::string(digits_.data(), digits_.size());
        }

        // Returns the masked number, showing only the last four digits.
        [[nodiscard]] std::string masked() const {
            return "**** **** **** " + std::string(digits_.data() + 12, 4);
        }

        constexpr bool operator==(const CardNumber&) const noexcept = default;
        constexpr bool operator!=(const CardNumber&) const noexcept = default;

    private:
        std::array<char, 16> digits_{};
    };
    
} // namespace atm::domain

#endif // ATM_DOMAIN_CARD_NUMBER_HPP
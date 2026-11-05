#ifndef ATM_DOMAIN_CARD_NUMBER_HPP
#define ATM_DOMAIN_CARD_NUMBER_HPP

#include <array>
#include <string>

namespace atm::domain {

    class CardNumber {
    public:
        constexpr CardNumber() noexcept { digits_.fill('0'); };

        [[nodiscard]] static CardNumber parse(const std::string& str);

        [[nodiscard]] std::string str() const {
            return std::string(digits_.data(), digits_.size());
        }

        [[nodiscard]] std::string masked() const {
            return "**** **** **** " + std::string(digits_.data() + 12, 4);
        }

        [[nodiscard]] std::string format() const;

        constexpr bool operator==(const CardNumber&) const noexcept = default;
        constexpr bool operator!=(const CardNumber&) const noexcept = default;

    private:
        std::array<char, 16> digits_{};
    };
    
} // namespace atm::domain

#endif // ATM_DOMAIN_CARD_NUMBER_HPP
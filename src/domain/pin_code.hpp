#ifndef ATM_DOMAIN_PIN_CODE_HPP
#define ATM_DOMAIN_PIN_CODE_HPP

#include <array>
#include <string>

namespace atm::domain {

    class PinCode {
    public:
        constexpr PinCode() noexcept { digits_.fill('0'); };
        
        [[nodiscard]] static PinCode parse(const std::string& str);

        [[nodiscard]] std::string str() const { return std::string(digits_.data(), digits_.size()); }
        [[nodiscard]] const std::array<char, 4>& digits() const noexcept { return digits_; }

        [[nodiscard]] constexpr bool operator==(const PinCode&) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const PinCode&) const noexcept = default;

        friend std::ostream& operator<<(std::ostream&, const PinCode&) = delete;

    private:
        std::array<char, 4> digits_{};
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_PIN_CODE_HPP
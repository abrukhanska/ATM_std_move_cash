#include "domain/pin_code.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

namespace atm::domain {
    [[nodiscard]] PinCode PinCode::parse(const std::string& str) {
        if (str.size() != 4) {
            throw std::invalid_argument("PinCode must be 4 digits");
        }
        PinCode result;
        for (std::size_t i = 0; i < 4; ++i) {
            const char c = str[i];
            if (c < '0' || c > '9') {
                throw std::invalid_argument("PinCode must contain only digits");
            }
            result.digits_[i] = c;
        }
        return result;
    }
}
#include "domain/card_number.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

namespace atm::domain {

    CardNumber CardNumber::parse(const std::string& str) {
        if (str.size() != 16) {
            throw std::invalid_argument("CardNumber must be 16 digits");
        }
        CardNumber result;
        for (std::size_t i = 0; i < 16; ++i) {
            const char c = str[i];
            if (c < '0' || c > '9') {
                throw std::invalid_argument("CardNumber must contain only digits");
            }
            result.digits_[i] = c;
        }
        return result;
    }

    std::string CardNumber::format() const {
        std::string result;
        for (std::size_t i = 0; i < 16; ++i) {
            if (i > 0 && i % 4 == 0) {
                result.push_back(' ');
            }
            result.push_back(digits_[i]);
        }
        return result;
    }

}  // namespace atm::domain
#include "domain/card_number.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

namespace atm::domain {

    CardNumber CardNumber::parse(const std::string_view str) {
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

}  // namespace atm::domain
#ifndef ATM_APPLICATION_PIN_HASHER_HPP
#define ATM_APPLICATION_PIN_HASHER_HPP

#include <cstdint>
#include <string_view>
#include <stdexcept>
#include <string>

namespace atm::application {

    class PinHasher {
    public:
        explicit PinHasher(std::uint32_t iterations = 10000)
            : iterations_(iterations) {
                if(iterations_ == 0) throw std::invalid_argument("PinHasher iterations must be greater than zero");
            }

        [[nodiscard]] std::string hash(std::string_view pin) const;
        [[nodiscard]] bool matches(std::string_view pin, std::string_view hash) const;

    private:
        std::uint32_t iterations_;
    };

} // namespace atm::application

#endif  // ATM_APPLICATION_PIN_HASHER_HPP

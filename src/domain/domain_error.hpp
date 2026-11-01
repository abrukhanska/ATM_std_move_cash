#ifndef ATM_DOMAIN_DOMAIN_ERROR_HPP
#define ATM_DOMAIN_DOMAIN_ERROR_HPP

#include <stdexcept>
#include <string>

#include "domain/enums.hpp"

namespace atm::domain {
	// An operation rule violation. The presentation layer selects the user message based on reason().
	// what() contains only the reason name, without PINs, hashes, card numbers, or entered amounts.
	class DomainError : public std::runtime_error {
    public:
        explicit DomainError(ErrorReason reason)
            : std::runtime_error(std::string("DomainError: ").append(toString(reason))),
            reason_(reason) {
        }

        [[nodiscard]] ErrorReason reason() const noexcept { return reason_; }

    private:
        ErrorReason reason_;
    };
}  // namespace atm::domain

#endif  // ATM_DOMAIN_DOMAIN_ERROR_HPP
#ifndef ATM_DOMAIN_PAYEE_HPP
#define ATM_DOMAIN_PAYEE_HPP

#include <cstdint>
#include <string>

#include "domain/enums.hpp"

namespace atm::domain {

    // A payee from the in-code directory. The struct itself does not enforce
    // anything: PaymentService validates the directory (ids positive and unique,
    // names not empty, categories valid).
    struct Payee {
        std::int64_t id{ 0 };
        std::string name;
        PayeeCategory category{ PayeeCategory::MobileOperator };
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_PAYEE_HPP
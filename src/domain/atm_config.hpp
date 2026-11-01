#ifndef ATM_DOMAIN_ATM_CONFIG_HPP
#define ATM_DOMAIN_ATM_CONFIG_HPP

#include <cstddef>
#include <vector>

#include "domain/money.hpp"

namespace atm::domain {

    // ATM settings. main creates a const instance; services borrow it through
    // const AtmConfig& and validate the values they use in their constructors.
    // The PIN attempt limit is fixed by the requirements and is not configurable.
    struct AtmConfig {
        Money dailyWithdrawalLimit = Money::uah(10'000);
        std::vector<Money> quickAmounts{
            Money::uah(200), Money::uah(500), Money::uah(1000), Money::uah(2000) };
        std::size_t journalTailSize = 20;
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_ATM_CONFIG_HPP
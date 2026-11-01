#ifndef ATM_APPLICATION_PLAN_DISPENSE_HPP
#define ATM_APPLICATION_PLAN_DISPENSE_HPP

#include "domain/banknote_set.hpp"
#include "domain/money.hpp"

namespace atm::application {

    // Selects an exact banknote combination without changing either argument.
    // Tries larger denominations first, using as many notes as possible.
    // Backtracks to smaller counts when the remaining amount cannot be formed.
    // The result totals amount and does not exceed available for any denomination.
    //
    // DomainError(InvalidAmount): amount is zero or not a multiple of 100 UAH.
    // DomainError(AtmOutOfCash): no exact combination is available.
    // Account balances and daily withdrawal limits are checked by the service.
    [[nodiscard]] domain::BanknoteSet planDispense(
        domain::Money amount,
        const domain::BanknoteSet& available
    );

}  // namespace atm::application

#endif  // ATM_APPLICATION_PLAN_DISPENSE_HPP

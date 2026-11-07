#ifndef ATM_APPLICATION_DETAIL_ACCESS_HPP
#define ATM_APPLICATION_DETAIL_ACCESS_HPP

#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/types.hpp"

namespace atm::domain {
    class IStorage;
}

namespace atm::application::detail {

    // Access checks only read from storage and return independent values.
    // Unknown card: UnknownCard. Wrong kind or inactive client: AccessDenied.
    [[nodiscard]] domain::Card requireActiveClient(
        domain::IStorage& storage, const domain::CardNumber& actor
    );

    // Unknown card: UnknownCard. Non-service card: AccessDenied.
    [[nodiscard]] domain::Card requireOperator(
        domain::IStorage& storage, const domain::CardNumber& actor
    );

    // Unknown account: UnknownAccount. Wrong owner: AccessDenied.
    [[nodiscard]] domain::Account requireOwnedAccount(
        domain::IStorage& storage, const domain::CardNumber& actor, domain::AccountId id
    );

}  // namespace atm::application::detail

#endif  // ATM_APPLICATION_DETAIL_ACCESS_HPP

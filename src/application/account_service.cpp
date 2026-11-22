#include "application/account_service.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "application/detail/access.hpp"

namespace atm::application {
    using domain::DomainError;
    using domain::ErrorReason;
    using domain::CardKind;
    using domain::CardStatus;

    std::vector<domain::Account> AccountService::accountsOf(const domain::CardNumber& actor) const
    {
        (void)detail::requireActiveClient(storage_, actor);
        return storage_.accounts().listByCard(actor);
    }

    domain::Money AccountService::balance(const domain::CardNumber& actor, domain::AccountId accountId) const
    {
        (void)detail::requireActiveClient(storage_, actor);
        const auto account = detail::requireOwnedAccount(storage_, actor, accountId);
        return account.balance();
    }

} // namespace atm::application

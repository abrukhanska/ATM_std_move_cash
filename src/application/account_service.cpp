#include "application/account_service.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

namespace atm::application {
    using domain::DomainError;
    using domain::ErrorReason;
    using domain::CardKind;
    using domain::CardStatus;

    std::vector<domain::Account> atm::application::AccountService::accountsOf(const domain::CardNumber &actor) const
    {
        const auto card = storage_.cards().find(actor);
        if(!card.has_value()) throw DomainError(ErrorReason::UnknownCard);
        if(card->kind() != CardKind::Client || card-> status() != CardStatus::Active) {
            throw DomainError(ErrorReason::AccessDenied);
        }
        return storage_.accounts().listByCard(actor);
    }

    domain::Money atm::application::AccountService::balance(const domain::CardNumber & actor, domain::AccountId accountId) const
    {
        const auto card = storage_.cards().find(actor);
        if(!card.has_value()) throw DomainError(ErrorReason::UnknownCard);
        if(card->kind() != CardKind::Client || card-> status() != CardStatus::Active) {
            throw DomainError(ErrorReason::AccessDenied);
        }

        const auto account = storage_.accounts().find(accountId);
        if(!account.has_value()) throw DomainError(ErrorReason::UnknownAccount);
        if(account->owner() != actor) {
            throw DomainError(ErrorReason::AccessDenied);
        }
        return account->balance();
    }

} // namespace atm::application

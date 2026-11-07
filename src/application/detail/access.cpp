#include "application/detail/access.hpp"

#include <utility>

#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/istorage.hpp"

namespace atm::application::detail {

    using domain::DomainError;
    using domain::ErrorReason;

    namespace {

        domain::Card requireCard(domain::IStorage& storage, const domain::CardNumber& actor) {
            auto card = storage.cards().find(actor);
            if (!card) {
                throw DomainError(ErrorReason::UnknownCard);
            }
            return std::move(*card);
        }

    }  // namespace

    domain::Card requireActiveClient(domain::IStorage& storage, const domain::CardNumber& actor) {
        auto card = requireCard(storage, actor);
        if (card.kind() != domain::CardKind::Client || card.status() != domain::CardStatus::Active) {
            throw DomainError(ErrorReason::AccessDenied);
        }
        return card;
    }

    domain::Card requireOperator(domain::IStorage& storage, const domain::CardNumber& actor) {
        auto card = requireCard(storage, actor);
        if (card.kind() != domain::CardKind::Service) {
            throw DomainError(ErrorReason::AccessDenied);
        }
        return card;
    }

    domain::Account requireOwnedAccount(
        domain::IStorage& storage, const domain::CardNumber& actor, domain::AccountId id
    ) {
        auto account = storage.accounts().find(id);
        if (!account) {
            throw DomainError(ErrorReason::UnknownAccount);
        }
        if (account->owner() != actor) {
            throw DomainError(ErrorReason::AccessDenied);
        }
        return std::move(*account);
    }

}  // namespace atm::application::detail

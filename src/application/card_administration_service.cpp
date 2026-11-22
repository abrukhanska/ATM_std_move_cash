#include "application/card_administration_service.hpp"
#include "domain/transaction.hpp"
#include "application/detail/access.hpp"

namespace atm::application {

    std::vector<domain::Card> CardAdministrationService::listRetained(const domain::CardNumber &actor) const
    {
        (void)detail::requireOperator(storage_, actor);
        return storage_.cards().listRetained();
    }

    domain::JournalEntry CardAdministrationService::release(const domain::CardNumber &actor, const domain::CardNumber &retainedCard)
    {
        domain::Transaction transaction(storage_);

        (void)detail::requireOperator(storage_, actor);

        auto target = storage_.cards().find(retainedCard);
        if (!target.has_value()) {
            throw domain::DomainError(domain::ErrorReason::UnknownCard);
        }
        if (target->kind() != domain::CardKind::Client || target->status() != domain::CardStatus::Retained) {
            throw domain::DomainError(domain::ErrorReason::AccessDenied);
        }

        target->release();

        const auto now = clock_();

        domain::JournalEntry entry;
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = domain::EntryKind::CardReleased;
        entry.actor = actor;
        entry.subjectCard = retainedCard;
        entry.amount = domain::Money::ofKopiykas(0);

        storage_.cards().save(*target);
        auto saved = storage_.journal().append(entry);
        transaction.commit();

        return saved;
    }

} // namespace atm::application

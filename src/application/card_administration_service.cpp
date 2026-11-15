#include "card_administration_service.hpp"
#include "transaction.hpp"

namespace atm::application {

    std::vector<domain::Card> CardAdministrationService::listRetained(const domain::CardNumber &actor) const
    {
        const auto card = storage_.cards().find(actor);
        if (!card.has_value()) throw domain::DomainError(domain::ErrorReason::UnknownCard);
        if (card->kind() != domain::CardKind::Service || card->status() != domain::CardStatus::Active) {
            throw domain::DomainError(domain::ErrorReason::AccessDenied);
        }

        return storage_.cards().listRetained();
    }

    domain::JournalEntry CardAdministrationService::release(const domain::CardNumber &actor, const domain::CardNumber &retainedCard)
    {
        const auto card = storage_.cards().find(actor);
        if (!card.has_value()) throw domain::DomainError(domain::ErrorReason::UnknownCard);
        if (card->kind() != domain::CardKind::Service || card->status() != domain::CardStatus::Active) {
            throw domain::DomainError(domain::ErrorReason::AccessDenied);
        }

        auto target = storage_.cards().find(retainedCard);
        if (!target.has_value()) {
            throw domain::DomainError(domain::ErrorReason::UnknownCard);
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

        domain::Transaction transaction(storage_);
        storage_.cards().save(*target);
        auto saved = storage_.journal().append(entry);
        transaction.commit();

        return saved;
    }

} // namespace atm::application

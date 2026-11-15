#include "application/device_service.hpp"
#include "domain/transaction.hpp"

namespace atm::application {

    domain::JournalEntry DeviceService::setInService(const domain::CardNumber actor, bool enable)
    {
        const auto card = storage_.cards().find(actor);
        if (!card.has_value()) throw domain::DomainError(domain::ErrorReason::UnknownCard);
        if (card->kind() != domain::CardKind::Service || card->status() != domain::CardStatus::Active) {
            throw domain::DomainError(domain::ErrorReason::AccessDenied);
        }
        if (enable && storage_.broken()) {
            throw domain::DomainError(domain::ErrorReason::AccessDenied);
        }

        const auto now = clock_();

        domain::JournalEntry entry;
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = enable ? domain::EntryKind::DeviceInService : domain::EntryKind::DeviceOutOfService;
        entry.actor = actor;
        entry.amount = domain::Money::ofKopiykas(0);

        domain::Transaction transaction(storage_);
        auto saved = storage_.journal().append(entry);
        transaction.commit();

        inService_ = enable;
        return saved;
    }

} // namespace atm::application
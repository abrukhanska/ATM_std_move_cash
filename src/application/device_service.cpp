#include "application/device_service.hpp"
#include "domain/transaction.hpp"
#include "application/detail/access.hpp"

namespace atm::application {

    domain::JournalEntry DeviceService::setInService(const domain::CardNumber& actor, bool enable)
    {
        domain::Transaction transaction(storage_);

        (void)detail::requireOperator(storage_, actor);
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

        auto saved = storage_.journal().append(entry);
        transaction.commit();

        inService_ = enable;
        return saved;
    }

} // namespace atm::application

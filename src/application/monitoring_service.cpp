#include "application/monitoring_service.hpp"

#include "application/detail/access.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/istorage.hpp"

namespace atm::application {

    MonitoringService::MonitoringService(domain::IStorage& storage, const domain::AtmConfig& config)
        : storage_(storage), config_(config) {
        if (config_.journalTailSize == 0) {
            throw domain::DomainError(domain::ErrorReason::InvalidData);
        }
    }

    std::vector<domain::JournalEntry> MonitoringService::recent(const domain::CardNumber& actor) const {
        (void)detail::requireOperator(storage_, actor);
        return storage_.journal().last(config_.journalTailSize);
    }

    domain::Money MonitoringService::totalLoaded(const domain::CardNumber& actor) const {
        (void)detail::requireOperator(storage_, actor);
        return storage_.journal().sumAmount(domain::EntryKind::CashLoaded);
    }

    domain::Money MonitoringService::totalDeposited(const domain::CardNumber& actor) const {
        (void)detail::requireOperator(storage_, actor);
        return storage_.journal().sumAmount(domain::EntryKind::Deposit);
    }

    domain::Money MonitoringService::totalWithdrawn(const domain::CardNumber& actor) const {
        (void)detail::requireOperator(storage_, actor);
        return storage_.journal().sumAmount(domain::EntryKind::Withdrawal);
    }

}  // namespace atm::application

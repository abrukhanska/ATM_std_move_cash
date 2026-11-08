#ifndef ATM_APPLICATION_MONITORING_SERVICE_HPP
#define ATM_APPLICATION_MONITORING_SERVICE_HPP

#include <vector>

#include "domain/atm_config.hpp"
#include "domain/card_number.hpp"
#include "domain/journal_entry.hpp"
#include "domain/money.hpp"

namespace atm::domain {
    class IStorage;
}

namespace atm::application {

    class MonitoringService {
    public:
        // A zero journal tail size causes InvalidData.
        MonitoringService(domain::IStorage& storage, const domain::AtmConfig& config);
        MonitoringService(domain::IStorage&, const domain::AtmConfig&&) = delete;

        [[nodiscard]] std::vector<domain::JournalEntry> recent(const domain::CardNumber& actor) const;

        [[nodiscard]] domain::Money totalLoaded(const domain::CardNumber& actor) const;
        [[nodiscard]] domain::Money totalDeposited(const domain::CardNumber& actor) const;
        [[nodiscard]] domain::Money totalWithdrawn(const domain::CardNumber& actor) const;

    private:
        domain::IStorage& storage_;
        const domain::AtmConfig& config_;
    };

}  // namespace atm::application

#endif  // ATM_APPLICATION_MONITORING_SERVICE_HPP

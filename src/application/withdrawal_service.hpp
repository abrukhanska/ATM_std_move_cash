#ifndef ATM_APPLICATION_WITHDRAWAL_SERVICE_HPP
#define ATM_APPLICATION_WITHDRAWAL_SERVICE_HPP

#include <vector>

#include "domain/atm_config.hpp"
#include "domain/card_number.hpp"
#include "domain/journal_entry.hpp"
#include "domain/money.hpp"
#include "domain/types.hpp"

namespace atm::domain {
    class IStorage;
}

namespace atm::application {

    class WithdrawalService {
    public:
        WithdrawalService(
            domain::IStorage& storage, const domain::AtmConfig& config, domain::Clock clock
        );

        WithdrawalService(domain::IStorage&, const domain::AtmConfig&&, domain::Clock) = delete;

        [[nodiscard]] domain::JournalEntry withdraw(
            const domain::CardNumber& actor, domain::AccountId from, domain::Money amount
        );

        [[nodiscard]] std::vector<domain::Money> quickAmounts() const;

    private:
        domain::IStorage& storage_;
        const domain::AtmConfig& config_;
        domain::Clock clock_;
    };

}  // namespace atm::application

#endif  // ATM_APPLICATION_WITHDRAWAL_SERVICE_HPP

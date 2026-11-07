#ifndef ATM_APPLICATION_DEPOSIT_SERVICE_HPP
#define ATM_APPLICATION_DEPOSIT_SERVICE_HPP

#include "domain/banknote_set.hpp"
#include "domain/card_number.hpp"
#include "domain/journal_entry.hpp"
#include "domain/types.hpp"

namespace atm::domain {
    class IStorage;
}

namespace atm::application {

    class DepositService {
    public:
        DepositService(domain::IStorage& storage, domain::Clock clock);

        [[nodiscard]] domain::JournalEntry deposit(
            const domain::CardNumber& actor, domain::AccountId to, const domain::BanknoteSet& notes
        );

    private:
        domain::IStorage& storage_;
        domain::Clock clock_;
    };

}  // namespace atm::application

#endif  // ATM_APPLICATION_DEPOSIT_SERVICE_HPP

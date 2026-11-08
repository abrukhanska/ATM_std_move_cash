#ifndef ATM_APPLICATION_PAYMENT_SERVICE_HPP
#define ATM_APPLICATION_PAYMENT_SERVICE_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/journal_entry.hpp"
#include "domain/money.hpp"
#include "domain/payee.hpp"
#include "domain/types.hpp"

namespace atm::domain {
    class IStorage;
}

namespace atm::application {

    class PaymentService {
    public:
        PaymentService(domain::IStorage& storage, domain::Clock clock);

        PaymentService(
            domain::IStorage& storage, domain::Clock clock, std::vector<domain::Payee> payees
        );

        [[nodiscard]] std::vector<domain::Payee> payees(domain::PayeeCategory category) const;

        // Saves the debit and journal entry in one transaction.
        [[nodiscard]] domain::JournalEntry pay(
            const domain::CardNumber& actor, domain::AccountId from,
            std::int64_t payeeId, std::string_view requisite, domain::Money amount
        );

    private:
        domain::IStorage& storage_;
        domain::Clock clock_;
        std::vector<domain::Payee> payees_;
    };

}

#endif
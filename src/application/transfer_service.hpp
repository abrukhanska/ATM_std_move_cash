#ifndef ATM_APPLICATION_TRANSFER_SERVICE_HPP
#define ATM_APPLICATION_TRANSFER_SERVICE_HPP

#include <string>

#include "domain/card_number.hpp"
#include "domain/journal_entry.hpp"
#include "domain/money.hpp"
#include "domain/types.hpp"

namespace atm::domain {
    class IStorage;
}

namespace atm::application {

    struct TransferPreview {
        domain::CardNumber toCard;
        domain::AccountId toAccount{ 0 };
        std::string recipientName;
        domain::Money amount;
    };

    class TransferService {
    public:
        TransferService(domain::IStorage& storage, domain::Clock clock);

        [[nodiscard]] TransferPreview preview(
            const domain::CardNumber& actor, domain::AccountId from,
            const domain::CardNumber& toCard, domain::Money amount
        ) const;

        [[nodiscard]] domain::JournalEntry execute(
            const domain::CardNumber& actor, domain::AccountId from, const TransferPreview& preview
        );

    private:
        domain::IStorage& storage_;
        domain::Clock clock_;
    };

}  // namespace atm::application

#endif  // ATM_APPLICATION_TRANSFER_SERVICE_HPP

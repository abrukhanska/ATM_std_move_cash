#ifndef ATM_APPLICATION_CARD_ADMINISTRATION_SERVICE_HPP
#define ATM_APPLICATION_CARD_ADMINISTRATION_SERVICE_HPP

#include "domain/istorage.hpp"

namespace atm::application {

    class CardAdministrationService {
    public:
        CardAdministrationService(domain::IStorage& storage, domain::Clock clock)
            : storage_(storage), clock_(std::move(clock)) {}

        [[nodiscard]] std::vector<domain::Card> listRetained(const domain::CardNumber& actor) const;
        domain::JournalEntry release(const domain::CardNumber& actor, const domain::CardNumber& retainedCard);

    private:
        domain::IStorage& storage_;
        domain::Clock clock_;
    };

} // namespace atm::application

#endif // ATM_APPLICATION_CARD_ADMINISTRATION_SERVICE_HPP

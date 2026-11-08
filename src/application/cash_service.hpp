#ifndef ATM_APPLICATION_CASH_SERVICE_HPP
#define ATM_APPLICATION_CASH_SERVICE_HPP

#include "domain/banknote_set.hpp"
#include "domain/card_number.hpp"
#include "domain/journal_entry.hpp"
#include "domain/types.hpp"

namespace atm::domain {
    class IStorage;
}

namespace atm::application {

    class CashService {
    public:
        CashService(domain::IStorage& storage, domain::Clock clock);

        // Reads cassette counts without changing storage.
        [[nodiscard]] domain::BanknoteSet inventory(const domain::CardNumber& actor) const;

        [[nodiscard]] domain::JournalEntry load(const domain::CardNumber& actor, const domain::BanknoteSet& notes);

        [[nodiscard]] domain::JournalEntry unload(const domain::CardNumber& actor, const domain::BanknoteSet& notes);

    private:
        enum class Operation { Load, Unload };

        [[nodiscard]] domain::JournalEntry apply(const domain::CardNumber& actor, const domain::BanknoteSet& notes,
            Operation operation);

        domain::IStorage& storage_;
        domain::Clock clock_;
    };

}

#endif  // ATM_APPLICATION_CASH_SERVICE_HPP

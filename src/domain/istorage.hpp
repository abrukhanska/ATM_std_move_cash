#ifndef ATM_DOMAIN_ISTORAGE_HPP
#define ATM_DOMAIN_ISTORAGE_HPP

#include "icard_repository.hpp"
#include "iaccount_repository.hpp"
#include "icash_repository.hpp"
#include "ijournal_repository.hpp"

namespace atm::domain {

    class IStorage {
    public:
        virtual ~IStorage() noexcept = default;

        [[nodiscard]] virtual ICardRepository& cards() = 0;
        [[nodiscard]] virtual IAccountRepository& accounts() = 0;
        [[nodiscard]] virtual ICashRepository& cash() = 0;
        [[nodiscard]] virtual IJournalRepository& journal() = 0;

        virtual void begin() = 0;
        virtual void commit() = 0;
        virtual void rollback() noexcept = 0;
        [[nodiscard]] virtual bool broken() const noexcept = 0;
    };

} // namespace atm::domain

#endif // ATM_DOMAIN_ISTORAGE_HPP
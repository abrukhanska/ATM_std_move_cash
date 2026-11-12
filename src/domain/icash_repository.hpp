#ifndef ATM_DOMAIN_ICASH_REPOSITORY_HPP
#define ATM_DOMAIN_ICASH_REPOSITORY_HPP

#include "domain/banknote_set.hpp"

namespace atm::domain {

    class ICashRepository {
    public:
        virtual ~ICashRepository() noexcept = default;

        [[nodiscard]] virtual BanknoteSet load() const = 0;
        virtual void save(const BanknoteSet& banknotes) = 0;
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_ICASH_REPOSITORY_HPP
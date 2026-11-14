#ifndef ATM_DOMAIN_IACCOUNT_REPOSITORY_HPP
#define ATM_DOMAIN_IACCOUNT_REPOSITORY_HPP

#include <optional>
#include "domain/account.hpp"

namespace atm::domain {

    class IAccountRepository {
    public:
        virtual ~IAccountRepository() noexcept = default;

        [[nodiscard]] virtual std::optional<Account> find(AccountId id) const = 0;
        [[nodiscard]] virtual std::optional<Account> findPrimary(const CardNumber& cardNumber) const = 0;
        [[nodiscard]] virtual std::vector<Account> listByCard(const CardNumber& cardNumber) const = 0;
        virtual void save(const Account& account) = 0;
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_IACCOUNT_REPOSITORY_HPP
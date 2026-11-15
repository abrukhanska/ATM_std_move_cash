#ifndef ATM_APPLICATION_ACCOUNT_SERVICE_HPP
#define ATM_APPLICATION_ACCOUNT_SERVICE_HPP

#include "domain/account.hpp"
#include "domain/istorage.hpp"

namespace atm::application {

    class AccountService {
    public:
        explicit AccountService(domain::IStorage& storage) noexcept
            :storage_(storage) {}

        [[nodiscard]] std::vector<domain::Account> accountsOf(const domain::CardNumber& actor) const;
        [[nodiscard]] domain::Money balance(const domain::CardNumber& actor, domain::AccountId accountId) const;

    private:
        domain::IStorage& storage_;
    };

} // namespace atm::application

#endif // ATM_STD_MOVE_CASH_APPLICATION_ACCOUNT_SERVICE_HPP
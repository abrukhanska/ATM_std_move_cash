#ifndef ATM_DOMAIN_ACCOUNT_HPP
#define ATM_DOMAIN_ACCOUNT_HPP

#include "card_number.hpp"
#include "money.hpp"
#include "types.hpp"

namespace atm::domain {
    
    class Account {
    public:
        Account(AccountId id, CardNumber owner, Money balance, bool primary = false)
            : id_(id), owner_(std::move(owner)), balance_(balance), primary_(primary) {
                if (id <= 0) {
                    throw std::invalid_argument("AccountId must be positive");
                }
        }

        [[nodiscard]] AccountId id() const noexcept { return id_; }
        [[nodiscard]] const CardNumber& owner() const noexcept { return owner_; }
        [[nodiscard]] Money balance() const noexcept { return balance_; }
        [[nodiscard]] bool isPrimary() const noexcept { return primary_; }

        void debit(Money amount);
        void credit(Money amount);

    private:
        AccountId id_{0};
        CardNumber owner_;
        Money balance_;
        bool primary_{false};
    };

}

#endif  // ATM_DOMAIN_ACCOUNT_HPP
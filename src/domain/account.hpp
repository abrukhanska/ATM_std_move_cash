#ifndef ATM_DOMAIN_ACCOUNT_HPP
#define ATM_DOMAIN_ACCOUNT_HPP

#include "card_number.hpp"
#include "money.hpp"
#include "types.hpp"

namespace atm::domain {
    
    class Account {
    public:
        Account(AccountId id, CardNumber cardNumber, Money balance, bool isPrimary = false)
            : id_(id), cardNumber_(std::move(cardNumber)), balance_(balance), isPrimary_(isPrimary) {
                if (id <= 0) {
                    throw std::invalid_argument("AccountId must be positive");
                }
        }

        [[nodiscard]] AccountId id() const noexcept { return id_; }
        [[nodiscard]] const CardNumber& cardNumber() const noexcept { return cardNumber_; }
        [[nodiscard]] Money balance() const noexcept { return balance_; }
        [[nodiscard]] bool isPrimary() const noexcept { return isPrimary_; }

        void debit(Money amount);
        void credit(Money amount);

    private:
        AccountId id_;
        CardNumber cardNumber_;
        Money balance_;
        bool isPrimary_{false};
    };

}

#endif  // ATM_DOMAIN_ACCOUNT_HPP
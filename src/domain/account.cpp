#include "domain/account.hpp"

namespace atm::domain {
    void Account::debit(Money amount) {
        if (balance_ < amount) {
            throw DomainError(ErrorReason::InsufficientFunds);
        }
        balance_ = balance_ - amount;
    }

    void Account::credit(Money amount) {
        balance_ = balance_ + amount;
    }
}
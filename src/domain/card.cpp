#include "domain/card.hpp"

namespace atm::domain {

    void Card::registerFailedAttempts() noexcept {
        if (status_ == CardStatus::Active && kind_ == CardKind::Client) {
            ++failedAttempts_;
            if (failedAttempts_ >= 3) {
                status_ = CardStatus::Retained;
            }
        }
    }

    void Card::registerSuccess() noexcept {
        if (status_ == CardStatus::Active && kind_ == CardKind::Client) {
            failedAttempts_ = 0;
        }
    }

    void Card::release() noexcept {
        status_ = CardStatus::Active;
        failedAttempts_ = 0;
    }
}
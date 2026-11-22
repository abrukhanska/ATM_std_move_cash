#include "domain/card.hpp"

#include <stdexcept>

namespace atm::domain {

    // Client: Active (0–2 failed attempts) or Retained (3 failed attempts)
    // Service: always Active with 0 failed attempts
    Card::Card(CardNumber number, std::string holderName, std::string pinHash, CardKind kind, CardStatus status, std::uint8_t failedAttempts)
        : number_(std::move(number)), holderName_(std::move(holderName)), pinHash_(std::move(pinHash)), kind_(kind), status_(status), failedAttempts_(failedAttempts) {
            if (kind_ == CardKind::Client) {
                const bool active = (status_ == CardStatus::Active && failedAttempts_ <= 2);
                const bool retained = (status_ == CardStatus::Retained && failedAttempts_ == 3);

                if (!active && !retained) throw std::invalid_argument("Invalid client card status or failed attempts");
            } else if (kind_ == CardKind::Service) {
                if (status_ != CardStatus::Active || failedAttempts_ != 0) throw std::invalid_argument("Service card must be Active with 0 failed attempts");
            } else { throw std::invalid_argument("Unknown card kind"); }
        }

    void Card::registerFailedAttempt() noexcept {
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
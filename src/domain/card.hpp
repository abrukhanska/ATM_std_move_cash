#ifndef ATM_DOMAIN_CARD_HPP
#define ATM_DOMAIN_CARD_HPP

#include "card_number.hpp"
#include "enums.hpp"

namespace atm::domain {

    class Card {
    public:
        Card(CardNumber number, std::string pinHash, CardKind kind, CardStatus status = CardStatus::Active, std::uint32_t failedAttempts = 0)
            : number_(std::move(number)), pinHash_(std::move(pinHash)), kind_(kind), status_(status), failedAttempts_(failedAttempts) {
                if (kind_ == CardKind::Service) {
                    status_ = CardStatus::Active;
                    failedAttempts_ = 0;
                }
        }

        [[nodiscard]] const CardNumber number() const noexcept { return number_; }
        [[nodiscard]] const std::string& pinHash() const noexcept { return pinHash_; }
        [[nodiscard]] CardKind kind() const noexcept { return kind_; }
        [[nodiscard]] CardStatus status() const noexcept { return status_; }
        [[nodiscard]] std::uint32_t failedAttempts() const noexcept { return failedAttempts_; }

        void changePinHash(std::string newHash) noexcept { pinHash_.swap(newHash); }

        void registerFailedAttempts() noexcept;
        void registerSuccess() noexcept;
        void release() noexcept;

    private:
        CardNumber number_;
        std::string pinHash_;
        CardKind kind_{CardKind::Client};
        CardStatus status_{CardStatus::Active};
        std::uint32_t failedAttempts_{0};
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_CARD_HPP
#ifndef ATM_DOMAIN_MONEY_HPP
#define ATM_DOMAIN_MONEY_HPP

#include <compare>
#include <cstdint>
#include <limits>
#include <string>

#include "domain/domain_error.hpp"

namespace atm::domain {

    class Money {
    public:
        constexpr Money() noexcept = default;

        [[nodiscard]] static constexpr Money uah(std::int64_t hryvnias) {
            if (hryvnias < 0) {
                throw DomainError(ErrorReason::InvalidAmount);
            }
            if (hryvnias > kMaxKopiykas / kKopiykasPerUah) {
                throw DomainError(ErrorReason::Overflow);
            }
            return Money(hryvnias * kKopiykasPerUah);
        }

        [[nodiscard]] static constexpr Money ofKopiykas(std::int64_t kopiykas) {
            if (kopiykas < 0) {
                throw DomainError(ErrorReason::InvalidAmount);
            }
            return Money(kopiykas);
        }

        [[nodiscard]] constexpr std::int64_t kopiykas() const noexcept {
            return kopiykas_;
        }

        [[nodiscard]] std::string format() const;

        [[nodiscard]] constexpr Money operator+(Money rhs) const {
            if (kopiykas_ > kMaxKopiykas - rhs.kopiykas_) {
                throw DomainError(ErrorReason::Overflow);
            }
            return Money(kopiykas_ + rhs.kopiykas_);
        }

        [[nodiscard]] constexpr Money operator-(Money rhs) const {
            if (kopiykas_ < rhs.kopiykas_) {
                throw DomainError(ErrorReason::InvalidAmount);
            }
            return Money(kopiykas_ - rhs.kopiykas_);
        }

        [[nodiscard]] constexpr bool operator==(const Money&) const noexcept = default;
        [[nodiscard]] constexpr std::strong_ordering operator<=>(const Money&) const noexcept = default;

    private:
        explicit constexpr Money(std::int64_t kopiykas) noexcept
            : kopiykas_(kopiykas) {
        }

        static constexpr std::int64_t kKopiykasPerUah{ 100 };

        static constexpr std::int64_t kMaxKopiykas{
            (std::numeric_limits<std::int64_t>::max)()
        };

        std::int64_t kopiykas_{ 0 };
    };
}

#endif  // ATM_DOMAIN_MONEY_HPP
#include "domain/banknote_set.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "domain/domain_error.hpp"

namespace atm::domain {

    std::size_t BanknoteSet::indexOf(Denomination d) {
        if (!isValidDenomination(d)) {
            throw DomainError(ErrorReason::InvalidData);
        }
        return denominationIndex(d);
    }

    std::uint32_t BanknoteSet::count(Denomination d) const {
        return counts_[indexOf(d)];
    }

    Money BanknoteSet::total() const {
        Money sum;

        for (std::size_t i = 0; i < kDenominationCount; ++i) {
            // Convert to int64_t before multiplication.
            // Multiplying a uint32_t count by 2000 in uint32_t could overflow.
            const auto hryvnias =
                static_cast<std::int64_t>(counts_[i])
                * denominationUah(kAllDenominations[i]);

            sum = sum + Money::uah(hryvnias);
        }

        return sum;
    }

    BanknoteSet BanknoteSet::with(
        Denomination d,
        std::uint32_t n
    ) const {
        // Validate the denomination before copying the set.
        const std::size_t index = indexOf(d);

        BanknoteSet result = *this;
        result.counts_[index] = n;

        return result;
    }

    BanknoteSet BanknoteSet::plus(const BanknoteSet& other) const {
        constexpr std::uint32_t kMax =
            (std::numeric_limits<std::uint32_t>::max)();

        // Validate all counts before constructing the result.
        for (std::size_t i = 0; i < kDenominationCount; ++i) {
            if (counts_[i] > kMax - other.counts_[i]) {
                throw DomainError(ErrorReason::Overflow);
            }
        }

        BanknoteSet result;

        for (std::size_t i = 0; i < kDenominationCount; ++i) {
            result.counts_[i] = counts_[i] + other.counts_[i];
        }

        return result;
    }

    BanknoteSet BanknoteSet::minus(const BanknoteSet& other) const {
        // Check availability for every denomination before constructing the result.
        for (std::size_t i = 0; i < kDenominationCount; ++i) {
            if (counts_[i] < other.counts_[i]) {
                throw DomainError(ErrorReason::InvalidAmount);
            }
        }

        BanknoteSet result;

        for (std::size_t i = 0; i < kDenominationCount; ++i) {
            result.counts_[i] = counts_[i] - other.counts_[i];
        }

        return result;
    }

}  // namespace atm::domain
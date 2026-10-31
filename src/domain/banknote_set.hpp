#ifndef ATM_DOMAIN_BANKNOTE_SET_HPP
#define ATM_DOMAIN_BANKNOTE_SET_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/enums.hpp"
#include "domain/money.hpp"

namespace atm::domain {

    // Stores the number of banknotes for each of the five denominations.
    // with/plus/minus return a new set and leave their operands unchanged.
    // Validation is completed before constructing the result.
    class BanknoteSet {
    public:
        // An empty set is valid, for example as part of a JournalEntry.
        constexpr BanknoteSet() noexcept = default;

        // Returns the number of banknotes of denomination d.
        // An invalid denomination causes DomainError(InvalidData).
        [[nodiscard]] std::uint32_t count(Denomination d) const;

        // Calculates the total amount using Money.
        // The maximum total for the current five denominations fits in int64_t.
        // Money's overflow checks provide additional protection.
        [[nodiscard]] Money total() const;

        // Returns a new set with the count for denomination d REPLACED by n.
        // Does not add n to the existing count.
        // An invalid denomination causes DomainError(InvalidData).
        [[nodiscard]] BanknoteSet with(
            Denomination d,
            std::uint32_t n
        ) const;

        // Adds the counts for each denomination.
        // A uint32_t overflow causes DomainError(Overflow).
        [[nodiscard]] BanknoteSet plus(const BanknoteSet& other) const;

        // Subtracts the counts for each denomination.
        // Insufficient banknotes cause DomainError(InvalidAmount).
        [[nodiscard]] BanknoteSet minus(const BanknoteSet& other) const;

        friend bool operator==(const BanknoteSet& lhs, const BanknoteSet& rhs) = default;

    private:
        // Converts a denomination to an array index after validating it.
        [[nodiscard]] static std::size_t indexOf(Denomination d);

        std::array<std::uint32_t, kDenominationCount> counts_{};
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_BANKNOTE_SET_HPP
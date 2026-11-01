#include "application/plan_dispense.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

namespace atm::application {

    namespace {

        using domain::BanknoteSet;
        using domain::DomainError;
        using domain::ErrorReason;
        using domain::Money;

        constexpr std::size_t kLevels = domain::kDenominationCount;
        constexpr std::int64_t kUahPerUnit = 100;
        constexpr std::int64_t kKopiykasPerUnit = 10'000;

        // Search from the largest denomination to the smallest.
        constexpr auto kDescending = [] {
            auto denominations = domain::kAllDenominations;
            for (std::size_t i = 0; i < kLevels; ++i) {
                denominations[i] = domain::kAllDenominations[kLevels - 1 - i];
            }
            return denominations;
            }();

        // The search requires positive, descending, whole-unit denominations.
        static_assert([] {
            for (std::size_t i = 0; i < kLevels; ++i) {
                const auto value = domain::denominationUah(kDescending[i]);
                if (value <= 0 || value % kUahPerUnit != 0) {
                    return false;
                }
                if (i > 0 && domain::denominationUah(kDescending[i - 1]) <= value) {
                    return false;
                }
            }
            return true;
            }());

        class Solver {
        public:
            explicit Solver(const BanknoteSet& available) {
                // All amounts below are measured in units of 100 UAH.
                for (std::size_t end = kLevels; end > 0; --end) {
                    const std::size_t i = end - 1;
                    unit_[i] = domain::denominationUah(kDescending[i]) / kUahPerUnit;
                    available_[i] = static_cast<std::int64_t>(available.count(kDescending[i]));
                    capacityFrom_[i] = capacityFrom_[i + 1] + available_[i] * unit_[i];
                    unitSumFrom_[i] = unitSumFrom_[i + 1] + unit_[i];
                }
            }

            [[nodiscard]] bool solve(std::size_t level, std::int64_t rest) {
                if (level == kLevels) {
                    return rest == 0;
                }
                if (rest > capacityFrom_[level]) {
                    return false;
                }

                const auto value = unit_[level];
                const auto maxTake = (std::min)(available_[level], rest / value);

                // Lower denominations must be able to cover the remainder.
                const auto mustCover = rest - capacityFrom_[level + 1];
                const std::int64_t capacityMin =
                    mustCover > 0 ? (mustCover + value - 1) / value : 0;

                // A feasible maximum count differs from maxTake by less than
                // the sum of the lower denomination values (in 100-UAH units).
                // Otherwise, some lower denomination supplies at least value
                // notes; exchanging that group increases the current count.
                // For 20, 10, 5, 2, 1 this gives at most 18, 8, 3, 1, 1 trials.
                const auto lowerUnitSum = unitSumFrom_[level + 1];
                const std::int64_t maxBacktrack = lowerUnitSum > 0 ? lowerUnitSum - 1 : 0;
                const auto minTake = (std::max)(capacityMin, maxTake - maxBacktrack);

                for (std::int64_t take = maxTake; take >= minTake; --take) {
                    taken_[level] = take;
                    if (solve(level + 1, rest - take * value)) {
                        return true;
                    }
                }

                taken_[level] = 0;
                return false;
            }

            [[nodiscard]] BanknoteSet result() const {
                BanknoteSet result;
                for (std::size_t i = 0; i < kLevels; ++i) {
                    // Successful counts are between zero and the uint32_t availability.
                    result = result.with(kDescending[i], static_cast<std::uint32_t>(taken_[i]));
                }
                return result;
            }

        private:
            std::array<std::int64_t, kLevels> unit_{};
            std::array<std::int64_t, kLevels> available_{};
            std::array<std::int64_t, kLevels + 1> capacityFrom_{};
            std::array<std::int64_t, kLevels + 1> unitSumFrom_{};
            std::array<std::int64_t, kLevels> taken_{};
        };

    }  // namespace

    domain::BanknoteSet planDispense(domain::Money amount, const domain::BanknoteSet& available) {
        if (amount.kopiykas() == 0 || amount.kopiykas() % kKopiykasPerUnit != 0) {
            throw DomainError(ErrorReason::InvalidAmount);
        }

        Solver solver(available);
        if (!solver.solve(0, amount.kopiykas() / kKopiykasPerUnit)) {
            throw DomainError(ErrorReason::AtmOutOfCash);
        }
        return solver.result();
    }

}  // namespace atm::application

#ifndef ATM_DOMAIN_ICARD_REPOSITORY_HPP
#define ATM_DOMAIN_ICARD_REPOSITORY_HPP

#include <optional>
#include <vector>
#include "domain/card.hpp"

namespace atm::domain {

    class ICardRepository {
    public:
        virtual ~ICardRepository() noexcept = default;

        [[nodiscard]] virtual std::optional<Card> find(const CardNumber& number) const = 0;
        [[nodiscard]] virtual std::vector<Card> listRetained() const = 0;
        virtual void save(const Card& card) = 0;
    };

}  // namespace atm::domain

#endif // ATM_DOMAIN_ICARD_REPOSITORY_HPP
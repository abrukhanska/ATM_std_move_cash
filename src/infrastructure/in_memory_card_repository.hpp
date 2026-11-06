#ifndef ATM_INFRASTRUCTURE_IN_MEMORY_CARD_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_IN_MEMORY_CARD_REPOSITORY_HPP

#include "domain/repositories.hpp"
#include <vector>

namespace atm::infrastructure {

class InMemoryCardRepository : public domain::ICardRepository {
public:
    [[nodiscard]] std::optional<domain::Card> find(const domain::CardNumber& number) const override {
        for (const auto& card : cards_) {
            if (card.number() == number) {
                return card;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<domain::Card> listRetained() const override {
        std::vector<domain::Card> result;
        for (const auto& card : cards_) {
            if (card.status() == domain::CardStatus::Retained) {
                result.push_back(card);
            }
        }
        return result;
    }

    void save(const domain::Card& card) override {
        for (auto& c : cards_) {
            if (c.number() == card.number()) {
                c = card;
                return;
            }
        }
        cards_.push_back(card);
    }

private:
    std::vector<domain::Card> cards_;
};

} // namespace atm::infrastructure

#endif // ATM_INFRASTRUCTURE_IN_MEMORY_CARD_REPOSITORY_HPP
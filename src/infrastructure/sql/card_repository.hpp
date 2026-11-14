#ifndef ATM_INFRASTRUCTURE_SQL_CARD_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_SQL_CARD_REPOSITORY_HPP

#include "domain/icard_repository.hpp"

namespace atm::infrastructure::sql {

    class Database;

    class CardRepository : public domain::ICardRepository {
    public:
        explicit CardRepository(Database& db) : db_(db) {}
        
        std::optional<domain::Card> find(const domain::CardNumber& number) const override;
        std::vector<domain::Card> listRetained() const override;
        void save(const domain::Card& card) override;

    private:
        Database& db_;
    };

} // atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_SQL_CARD_REPOSITORY_HPP
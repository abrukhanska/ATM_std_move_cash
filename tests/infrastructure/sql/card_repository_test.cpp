#include "support/mini_test.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/card_repository.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"

#include <filesystem>
#include <string>
#include <optional>

namespace {

using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::CardRepository;
using atm::domain::Card;
using atm::domain::CardNumber;
using atm::domain::CardKind;
using atm::domain::CardStatus;

TEST(DB2CardReporitory, FindCardReturnEmpty) {
    std::string db_path = "test_db.db";
    
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        CardRepository repo(db);

        std::string number = "0000123456789012";
        auto card_number = CardNumber::parse(number);
        
        EXPECT_EQ(card_number.str(), number);

        auto found_empty = repo.find(card_number);
        EXPECT_FALSE(found_empty.has_value());
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(DB2CardReporitory, FindUnknownCard) {
    std::string db_path = "test_db.db";
    
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    {
        Database db(db_path);
        CardRepository repo(db);

        auto unknown_number = CardNumber::parse("9999888877776666");
        auto result = repo.find(unknown_number);
        
        EXPECT_FALSE(result.has_value());
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

} // namespace
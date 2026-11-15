#include <filesystem>
#include <string>

#include "domain/banknote_set.hpp"
#include "domain/enums.hpp"
#include "infrastructure/sql/cash_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "support/mini_test.hpp"

namespace {

using atm::domain::BanknoteSet;
using atm::domain::Denomination;
using atm::infrastructure::sql::CashRepository;
using atm::infrastructure::sql::Database;

TEST(CashRepository, SavesLoadsAndOverwritesBanknotes) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        CashRepository repo(db);

        EXPECT_TRUE(repo.load() == BanknoteSet{});

        const BanknoteSet loaded = BanknoteSet{}
            .with(Denomination::UAH500, 10)
            .with(Denomination::UAH200, 5);
        repo.save(loaded);
        EXPECT_TRUE(repo.load() == loaded);

        const BanknoteSet overwritten = BanknoteSet{}
            .with(Denomination::UAH500, 3);
        repo.save(overwritten);
        EXPECT_TRUE(repo.load() == overwritten);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

} // namespace
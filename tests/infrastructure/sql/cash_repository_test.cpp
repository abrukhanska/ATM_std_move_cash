#include <filesystem>
#include <string>

#include <sqlite3.h>

#include "domain/banknote_set.hpp"
#include "domain/enums.hpp"
#include "infrastructure/sql/cash_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "support/mini_test.hpp"

namespace {

using atm::domain::BanknoteSet;
using atm::domain::Denomination;
using atm::infrastructure::sql::CashRepository;
using atm::infrastructure::sql::Database;
using atm::infrastructure::sql::StorageError;

TEST(CashRepository, SavesLoadsAndOverwritesBanknotes) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        CashRepository repository(db);

        EXPECT_TRUE(repository.load() == BanknoteSet{});

        const BanknoteSet denominations = BanknoteSet{}
            .with(Denomination::UAH100, 10)
            .with(Denomination::UAH200, 8)
            .with(Denomination::UAH500, 6)
            .with(Denomination::UAH1000, 4)
            .with(Denomination::UAH2000, 2);
        repository.save(denominations);
        EXPECT_TRUE(repository.load() == denominations);

        const BanknoteSet denominations2 = BanknoteSet{}
            .with(Denomination::UAH500, 3);
        repository.save(denominations2);
        EXPECT_TRUE(repository.load() == denominations2);

        repository.save(BanknoteSet{});
        EXPECT_TRUE(repository.load() == BanknoteSet{});
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(CashRepository, EnforcesConstraintsAndRejectsOutOfRangeCounts) {
    const std::string db_path = "test_db.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);

        int neg_rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO banknotes (denomination, count) VALUES (100, -1);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(neg_rc != SQLITE_OK);

        int rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO banknotes (denomination, count) VALUES (100, 5);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(rc == SQLITE_OK);

        int dup_rc = sqlite3_exec(
            db.handle(),
            "INSERT INTO banknotes (denomination, count) VALUES (100, 10);",
            nullptr, nullptr, nullptr);
        EXPECT_TRUE(dup_rc != SQLITE_OK);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        sqlite3* db_ptr = nullptr;
        EXPECT_TRUE(sqlite3_open(db_path.c_str(), &db_ptr) == SQLITE_OK);
        const char* create_sql =
            "CREATE TABLE banknotes ("
            "  denomination INTEGER PRIMARY KEY,"
            "  count INTEGER NOT NULL"
            ");";
        EXPECT_TRUE(sqlite3_exec(db_ptr, create_sql, nullptr, nullptr, nullptr) == SQLITE_OK);

        const char* insert_sql =
            "INSERT INTO banknotes (denomination, count) VALUES "
            "(100, 5000000000),"
            "(200, 0),"
            "(500, 0),"
            "(1000, 0),"
            "(2000, 0);";
        EXPECT_TRUE(sqlite3_exec(db_ptr, insert_sql, nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(db_ptr);

        Database db(db_path);
        CashRepository repository(db);

        EXPECT_THROW(static_cast<void>(repository.load()), StorageError);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

} // namespace
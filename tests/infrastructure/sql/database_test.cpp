#include "support/mini_test.hpp"
#include "infrastructure/sql/database.hpp"

#include <filesystem>
#include <string>

//without seed
namespace {

using atm::infrastructure::sql::Database;

TEST(DB1Database, DBInitialAndRepeatedLaunch) {
    std::string db_path = "test_db.db";
    
    //db does not exist before start
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        Database db(db_path);
        EXPECT_TRUE(db.handle() != nullptr);
    }

    EXPECT_TRUE(std::filesystem::exists(db_path));

    {
        Database db(db_path);
        EXPECT_TRUE(db.handle() != nullptr);
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(X3Datadase, InvalidDatabasePath) {
    std::string invalid_path = "fake_folder/subfolder/database.db";
    
    bool exception_thrown = false;
    try {
        Database db(invalid_path);
    } catch (const std::exception&) {
        exception_thrown = true;
    }
    
    EXPECT_TRUE(exception_thrown);
}

} // namespace
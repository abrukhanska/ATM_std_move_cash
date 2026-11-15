#include "support/financial_sql_fixture.hpp"
#include "application/card_administration_service.hpp"

namespace {
    using namespace atm::test_support::financial_sql;
    void checkRollback(int operation, const char* trigger) {
        Fixture f;
        (void)f.run(3);
        const auto before = snapshot(f.db);
        sql(f.db, trigger);
        std::string updateTrigger(trigger);
        const auto insertPosition = updateTrigger.find("BEFORE INSERT");
        const bool watchesSave = insertPosition != std::string::npos;
        if (watchesSave) {
            updateTrigger.replace(insertPosition, std::string("BEFORE INSERT").size(), "BEFORE UPDATE");
            const auto namePosition = updateTrigger.find("x1_failure");
            updateTrigger.replace(namePosition, std::string("x1_failure").size(), "x1_failure_update");
            sql(f.db, updateTrigger.c_str());
        }
        bool expectedFailure = false;
        try {
            (void)f.run(operation);
        }
        catch (const StorageError& error) {
            expectedFailure = std::string(error.what()).find("X1 injected") != std::string::npos;
        }
        EXPECT_TRUE(expectedFailure);
        EXPECT_FALSE(f.db.broken());
        EXPECT_TRUE(sqlite3_get_autocommit(f.db.handle()) != 0);
        EXPECT_EQ(snapshot(f.db), before);

        sql(f.db, "DROP TRIGGER x1_failure;");
        if (watchesSave) sql(f.db, "DROP TRIGGER x1_failure_update;");
        const auto saved = f.run(operation);
        EXPECT_TRUE(saved.id > 0);
        EXPECT_NE(snapshot(f.db), before);
        EXPECT_EQ(f.db.journal().last(10).size(), 2U);
    }

    TEST(X1Sql, JournalFailureRestoresAllTablesForEveryFinancialOperation) {
        for (int operation = 0; operation < 6; ++operation) {
            checkRollback(operation,
                "CREATE TRIGGER x1_failure AFTER INSERT ON journal "
                "BEGIN SELECT RAISE(ABORT, 'X1 injected journal failure'); END;");
        }
    }

    TEST(X1Sql, CashFailureRestoresEarlierAccountWrite) {
        for (int operation = 0; operation < 2; ++operation) {
            checkRollback(operation,
                "CREATE TRIGGER x1_failure BEFORE INSERT ON banknotes "
                "BEGIN SELECT RAISE(ABORT, 'X1 injected cash failure'); END;");
        }
    }

    TEST(X1Sql, RecipientFailureRestoresSenderAccount) {
        checkRollback(2,
            "CREATE TRIGGER x1_failure BEFORE INSERT ON accounts WHEN NEW.account_id = 3 "
            "BEGIN SELECT RAISE(ABORT, 'X1 injected recipient failure'); END;");
    }

    TEST(X1Sql, CardWriteIsRolledBackWhenJournalFails) {
        Fixture f;
        sql(f.db, "UPDATE cards SET status = 'Retained', failed_attempts = 3 "
            "WHERE card_number = '4000000000000001';");
        (void)f.run(4);
        const auto before = snapshot(f.db);
        sql(f.db, "CREATE TRIGGER x1_failure AFTER INSERT ON journal "
            "BEGIN SELECT RAISE(ABORT, 'X1 injected card journal failure'); END;");
        CardAdministrationService admin(f.db, f.clock);
        bool expectedFailure = false;
        try { (void)admin.release(f.service, f.client); }
        catch (const StorageError& error) {
            expectedFailure = std::string(error.what()).find("X1 injected") != std::string::npos;
        }
        EXPECT_TRUE(expectedFailure);
        EXPECT_EQ(snapshot(f.db), before);
        EXPECT_FALSE(f.db.broken());
        EXPECT_TRUE(sqlite3_get_autocommit(f.db.handle()) != 0);
        sql(f.db, "DROP TRIGGER x1_failure;");
        const auto entry = admin.release(f.service, f.client);
        EXPECT_EQ(f.db.cards().find(f.client)->status(), CardStatus::Active);
        EXPECT_EQ(f.db.cards().find(f.client)->failedAttempts(), 0);
        EXPECT_EQ(f.db.journal().last(1).front(), entry);
        EXPECT_EQ(f.db.journal().last(10).size(), 2U);
    }
}

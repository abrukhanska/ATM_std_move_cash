#ifndef ATM_TEST_SUPPORT_FINANCIAL_SQL_FIXTURE_HPP
#define ATM_TEST_SUPPORT_FINANCIAL_SQL_FIXTURE_HPP
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <sqlite3.h>
#include "application/cash_service.hpp"
#include "application/deposit_service.hpp"
#include "application/monitoring_service.hpp"
#include "application/payment_service.hpp"
#include "application/transfer_service.hpp"
#include "application/withdrawal_service.hpp"
#include "domain/transaction.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/data_seeder.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "support/mini_test.hpp"

namespace atm::test_support::financial_sql {
    using namespace atm::domain;
    using namespace atm::application;
    using namespace atm::infrastructure::sql;

    inline void sql(Database& db, const char* statement) {
        if (sqlite3_exec(db.handle(), statement, nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("SQL fixture setup failed: ") + sqlite3_errmsg(db.handle()));
        }
    }

    struct Fixture {
        Database db{ ":memory:" };
        const CardNumber client = CardNumber::parse("4000000000000001");
        const CardNumber recipient = CardNumber::parse("4000000000000002");
        const CardNumber service = CardNumber::parse("9000000000000001");
        const AtmConfig config;
        TimePoint now = fromEpochSeconds(1'768'471'200);
        int clockCalls = 0;
        Clock clock = [this] { ++clockCalls; return now; };
        const BanknoteSet notes = BanknoteSet{}.with(Denomination::UAH100, 1);
        Fixture() {
            seedDemoDataIfEmpty(db,
                { Card(client, "Client", "hash", CardKind::Client),
                  Card(recipient, "Recipient", "hash", CardKind::Client),
                  Card(service, "Operator", "hash", CardKind::Service) },
                { Account(1, client, Money::uah(20'000), true),
                  Account(2, client, Money::uah(5'000)),
                  Account(3, recipient, Money::uah(500), true) });
            validateDatabase(db);
            Transaction tx(db);
            db.cash().save(BanknoteSet{}.with(Denomination::UAH100, 200));
            tx.commit();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;
        Fixture(Fixture&&) = delete;
        Fixture& operator=(Fixture&&) = delete;

        JournalEntry run(int operation) {
            switch (operation) {
            case 0: return WithdrawalService(db, config, clock).withdraw(client, 1, Money::uah(100));
            case 1: return DepositService(db, clock).deposit(client, 1, notes);
            case 2: {
                TransferService transfer(db, clock);
                return transfer.execute(client, 1, transfer.preview(client, 1, recipient, Money::uah(100)));
            }
            case 3: return PaymentService(db, clock).pay(client, 1, 2, "", Money::uah(100));
            case 4: return CashService(db, clock).load(service, notes);
            case 5: return CashService(db, clock).unload(service, notes);
            default: throw std::logic_error("Unknown test operation");
            }
        }
    };

    using Snapshot = std::vector<std::vector<std::string>>;

    inline Snapshot snapshot(Database& db) {
        Snapshot result;
        const char* queries[] = {
            "SELECT * FROM cards ORDER BY card_number;",
            "SELECT * FROM accounts ORDER BY account_id;",
            "SELECT * FROM banknotes ORDER BY denomination;",
            "SELECT * FROM journal ORDER BY journal_id;"
        };
        for (const auto* query : queries) {
            result.push_back({ query });
            sqlite3_stmt* raw = nullptr;
            const int prepared = sqlite3_prepare_v2(db.handle(), query, -1, &raw, nullptr);
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
            if (prepared != SQLITE_OK) throw std::runtime_error("Snapshot prepare failed");
            int rc;
            while ((rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
                std::vector<std::string> row;
                for (int column = 0; column < sqlite3_column_count(statement.get()); ++column) {
                    const int type = sqlite3_column_type(statement.get(), column);
                    std::string value = std::to_string(type) + ":";
                    if (type != SQLITE_NULL) {
                        const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), column));
                        const int size = sqlite3_column_bytes(statement.get(), column);
                        if (!bytes) throw std::runtime_error("Snapshot conversion failed");
                        value.append(bytes, static_cast<std::size_t>(size));
                    }
                    row.push_back(value);
                }
                result.push_back(row);
            }
            if (rc != SQLITE_DONE) throw std::runtime_error("Snapshot step failed");
        }
        return result;
    }

}
#endif
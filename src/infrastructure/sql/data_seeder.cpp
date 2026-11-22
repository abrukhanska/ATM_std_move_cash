#include "data_seeder.hpp"
#include "domain/transaction.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "infrastructure/sql/stmt.hpp"

#include <sqlite3.h>
#include <limits>

namespace atm::infrastructure::sql {

    namespace {

        bool isEmpty(sqlite3* handle, const std::string& table) {
            const std::string sql = "SELECT COUNT(*) FROM " + table + ";";
            Stmt stmt = prepare(handle, sql.data(), "isEmpty prepare");

            int rc = sqlite3_step(stmt.get());
            if (rc != SQLITE_ROW) {
                throw StorageError("isEmpty step", rc, sqlite3_errmsg(handle));
            }

            return sqlite3_column_int64(stmt.get(), 0) == 0;
        }

        std::string extractString(sqlite3_stmt* stmt, int columnIndex, const char* context) {
            if (sqlite3_column_type(stmt, columnIndex) != SQLITE_TEXT) {
                throw StorageError(context, SQLITE_CORRUPT, "Expected TEXT value");
            }
            const auto* text_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, columnIndex));
            const int text_length = sqlite3_column_bytes(stmt, columnIndex);
            if (!text_ptr || text_length < 0) {
                throw StorageError(context, SQLITE_CORRUPT, "Failed to read TEXT value");
            }

            return std::string(text_ptr, static_cast<std::size_t>(text_length));
        }

        std::int64_t extractInt64(sqlite3_stmt* stmt, int columnIndex, const char* context) {
            if (sqlite3_column_type(stmt, columnIndex) != SQLITE_INTEGER) {
                throw StorageError(context, SQLITE_CORRUPT, "Expected INTEGER value");
            }
            return sqlite3_column_int64(stmt, columnIndex);
        }

        int extractInt(sqlite3_stmt* stmt, int columnIndex, const char* context) {
            const std::int64_t value = extractInt64(stmt, columnIndex, context);
            if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
                throw StorageError(context, SQLITE_RANGE, "INTEGER value out of int range");
            }
            return static_cast<int>(value);
        }

    } // namespace

    void seedDemoDataIfEmpty(Database& db, const std::vector<domain::Card>& cards, const std::vector<domain::Account>& accounts) {
        sqlite3* handle = db.handle();
        if (!isEmpty(handle, "cards") || !isEmpty(handle, "accounts") || !isEmpty(handle, "banknotes") || !isEmpty(handle, "journal")) {
            return;
        }

        domain::Transaction transaction(db);

        {
            const char* banknotes_sql = "INSERT INTO banknotes (denomination, count) VALUES (?, 0);";
            Stmt stmt = prepare(handle, banknotes_sql, "seedDemoDataIfEmpty banknotes prepare");

            for (domain::Denomination denomination : domain::kAllDenominations) {
                if (sqlite3_bind_int64(stmt.get(), 1, domain::denominationUah(denomination)) != SQLITE_OK) {
                    throw StorageError("seedDemoDataIfEmpty banknotes bind", SQLITE_ERROR, sqlite3_errmsg(handle));
                }

                int rc = sqlite3_step(stmt.get());
                if (rc != SQLITE_DONE) {
                    throw StorageError("seedDemoDataIfEmpty banknotes step", rc, sqlite3_errmsg(handle));
                }
                sqlite3_reset(stmt.get());
            }
        }

        {
            const char* cards_sql = "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES (?, ?, ?, ?, ?, ?);";
            Stmt stmt = prepare(handle, cards_sql, "seedDemoDataIfEmpty cards prepare");

            for (const domain::Card& card : cards) {
                std::string card_number = card.number().str();
                std::string_view kind = domain::toString(card.kind());
                std::string_view status = domain::toString(card.status());

                if (sqlite3_bind_text(stmt.get(), 1, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
                    sqlite3_bind_text(stmt.get(), 2, card.holderName().data(), static_cast<int>(card.holderName().size()), SQLITE_TRANSIENT) != SQLITE_OK ||
                    sqlite3_bind_text(stmt.get(), 3, card.pinHash().data(), static_cast<int>(card.pinHash().size()), SQLITE_TRANSIENT) != SQLITE_OK ||
                    sqlite3_bind_text(stmt.get(), 4, kind.data(), static_cast<int>(kind.size()), SQLITE_STATIC) != SQLITE_OK ||
                    sqlite3_bind_text(stmt.get(), 5, status.data(), static_cast<int>(status.size()), SQLITE_STATIC) != SQLITE_OK ||
                    sqlite3_bind_int(stmt.get(), 6, card.failedAttempts()) != SQLITE_OK) {

                    throw StorageError("seedDemoDataIfEmpty cards bind", SQLITE_ERROR, sqlite3_errmsg(handle));
                }

                int rc = sqlite3_step(stmt.get());
                if (rc != SQLITE_DONE) {
                    throw StorageError("seedDemoDataIfEmpty cards step", rc, sqlite3_errmsg(handle));
                }
                sqlite3_reset(stmt.get());
            }
        }

        {
            const char* accounts_sql = "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES (?, ?, ?, ?);";
            Stmt stmt = prepare(handle, accounts_sql, "seedDemoDataIfEmpty accounts prepare");

            for (const domain::Account& account : accounts) {
                std::string card_number = account.owner().str();

                if (sqlite3_bind_int64(stmt.get(), 1, account.id()) != SQLITE_OK ||
                    sqlite3_bind_text(stmt.get(), 2, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
                    sqlite3_bind_int64(stmt.get(), 3, account.balance().kopiykas()) != SQLITE_OK ||
                    sqlite3_bind_int(stmt.get(), 4, account.isPrimary() ? 1 : 0) != SQLITE_OK) {

                    throw StorageError("seedDemoDataIfEmpty accounts bind", SQLITE_ERROR, sqlite3_errmsg(handle));
                }

                int rc = sqlite3_step(stmt.get());
                if (rc != SQLITE_DONE) {
                    throw StorageError("seedDemoDataIfEmpty accounts step", rc, sqlite3_errmsg(handle));
                }
                sqlite3_reset(stmt.get());
            }
        }

        transaction.commit();
    }

    void validateDatabase(Database& db) {
        sqlite3* handle = db.handle();
        const auto max_uint32 = static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());

        {
            const char* sql = "SELECT denomination, count FROM banknotes;";
            Stmt stmt = prepare(handle, sql, "validateDatabase banknotes prepare");

            std::array<bool, domain::kDenominationCount> found_denominations{};
            std::size_t row_count = 0;
            int rc = SQLITE_OK;

            while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                const std::int64_t denomination = extractInt64(stmt.get(), 0, "validateDatabase banknotes denomination");
                const std::int64_t count = extractInt64(stmt.get(), 1, "validateDatabase banknotes count");

                if (count < 0 || count > max_uint32) {
                    throw StorageError("validateDatabase banknotes", SQLITE_RANGE, "Banknote count out of uint32_t range");
                }

                bool matched = false;
                for (std::size_t i = 0; i < domain::kDenominationCount; ++i) {
                    if (domain::denominationUah(domain::kAllDenominations[i]) == denomination) {
                        if (found_denominations[i]) {
                            throw StorageError("validateDatabase banknotes", SQLITE_CORRUPT, "Duplicate banknote denomination");
                        }
                        found_denominations[i] = true;
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    throw StorageError("validateDatabase banknotes", SQLITE_CORRUPT, "Unknown banknote denomination");
                }
                ++row_count;
            }

            if (rc != SQLITE_DONE) {
                throw StorageError("validateDatabase banknotes step", rc, sqlite3_errmsg(handle));
            }
            if (row_count != domain::kDenominationCount) {
                throw StorageError("validateDatabase banknotes", SQLITE_CORRUPT, "Missing banknote denominations");
            }
        }

        {
            const char* sql = R"(
                SELECT c.card_number, c.kind, c.status, c.failed_attempts,
                       COUNT(a.account_id) AS total_accounts,
                       COALESCE(SUM(CASE WHEN a.is_primary = 1 THEN 1 ELSE 0 END), 0) AS primary_accounts
                FROM cards c
                LEFT JOIN accounts a ON a.card_number = c.card_number
                GROUP BY c.card_number;
            )";

            Stmt stmt = prepare(handle, sql, "validateDatabase cards prepare");

            const std::string_view client = domain::toString(domain::CardKind::Client);
            const std::string_view service = domain::toString(domain::CardKind::Service);
            const std::string_view active = domain::toString(domain::CardStatus::Active);
            const std::string_view retained = domain::toString(domain::CardStatus::Retained);

            int rc = SQLITE_OK;
            while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                const std::string card_number = extractString(stmt.get(), 0, "validateDatabase cards card_number");
                const std::string kind = extractString(stmt.get(), 1, "validateDatabase cards kind");
                const std::string status = extractString(stmt.get(), 2, "validateDatabase cards status");
                const int failed_attempts = extractInt(stmt.get(), 3, "validateDatabase cards failed_attempts");
                const int total_accounts = extractInt(stmt.get(), 4, "validateDatabase cards total_accounts");
                const int primary_accounts = extractInt(stmt.get(), 5, "validateDatabase cards primary_accounts");

                try {
                    static_cast<void>(domain::CardNumber::parse(card_number));
                } catch (const std::invalid_argument&) {
                    throw StorageError("validateDatabase cards", SQLITE_CORRUPT, "Invalid card number format");
                }

                if (kind == client) {
                    const bool status_active = (status == active && failed_attempts >= 0 && failed_attempts <= 2);
                    const bool status_retained = (status == retained && failed_attempts == 3);
                    if (!status_active && !status_retained) {
                        throw StorageError("validateDatabase cards", SQLITE_CORRUPT, "Inconsistent client card status or attempts");
                    }
                    if (total_accounts < 1 || primary_accounts != 1) {
                        throw StorageError("validateDatabase cards", SQLITE_CORRUPT, "Client card must have at least 1 account and exactly 1 primary");
                    }
                } else if (kind == service) {
                    if (status != active || failed_attempts != 0) {
                        throw StorageError("validateDatabase cards", SQLITE_CORRUPT, "Inconsistent service card status or attempts");
                    }
                    if (total_accounts != 0) {
                        throw StorageError("validateDatabase cards", SQLITE_CORRUPT, "Service card must not have accounts");
                    }
                } else {
                    throw StorageError("validateDatabase cards", SQLITE_CORRUPT, "Unknown card kind");
                }
            }

            if (rc != SQLITE_DONE) {
                throw StorageError("validateDatabase cards step", rc, sqlite3_errmsg(handle));
            }
        }

        {
            const char* sql = R"(
                SELECT a.account_id, a.balance_kop, a.is_primary, c.card_number
                FROM accounts a
                LEFT JOIN cards c ON c.card_number = a.card_number;
            )";

            Stmt stmt = prepare(handle, sql, "validateDatabase accounts prepare");

            int rc = SQLITE_OK;
            while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                const std::int64_t id = extractInt64(stmt.get(), 0, "validateDatabase accounts account_id");
                const std::int64_t balance = extractInt64(stmt.get(), 1, "validateDatabase accounts balance_kop");
                const int is_primary = extractInt(stmt.get(), 2, "validateDatabase accounts is_primary");
                const bool has_owner = (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL);

                if (id <= 0 || balance < 0 || (is_primary != 0 && is_primary != 1) || !has_owner) {
                    throw StorageError("validateDatabase accounts", SQLITE_CORRUPT, "Invalid account record");
                }
            }

            if (rc != SQLITE_DONE) {
                throw StorageError("validateDatabase accounts step", rc, sqlite3_errmsg(handle));
            }
        }

        {
            const char* sql = R"(
                SELECT journal_id, at_utc, local_day, kind, actor_card, account_id, target_account_id, subject_card, amount_kop,
                       cash_100, cash_200, cash_500, cash_1000, cash_2000,
                       payee_id, requisite
                FROM journal;
            )";

            Stmt stmt = prepare(handle, sql, "validateDatabase journal prepare");

            int rc = SQLITE_OK;
            while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                const std::int64_t id = extractInt64(stmt.get(), 0, "validateDatabase journal journal_id");
                if (id <= 0) {
                    throw StorageError("validateDatabase journal", SQLITE_CORRUPT, "Invalid journal_id");
                }

                const std::int64_t at_utc = extractInt64(stmt.get(), 1, "validateDatabase journal at_utc");
                const std::string local_day = extractString(stmt.get(), 2, "validateDatabase journal local_day");
                const std::string kind = extractString(stmt.get(), 3, "validateDatabase journal kind");
                const std::string actor_str = extractString(stmt.get(), 4, "validateDatabase journal actor_card");

                const auto optional_kind = domain::toEntryKind(kind);
                if (!optional_kind.has_value()) {
                    throw StorageError("validateDatabase journal", SQLITE_CORRUPT, "Unknown journal kind: " + kind);
                }

                domain::JournalEntry entry;
                entry.id = id;
                entry.atUtc = at_utc;
                entry.localDay = local_day;
                entry.kind = *optional_kind;

                try {
                    entry.actor = domain::CardNumber::parse(actor_str);
                    if (sqlite3_column_type(stmt.get(), 5) != SQLITE_NULL) {
                        entry.account = extractInt64(stmt.get(), 5, "validateDatabase journal account_id");
                    }
                    if (sqlite3_column_type(stmt.get(), 6) != SQLITE_NULL) {
                        entry.target = extractInt64(stmt.get(), 6, "validateDatabase journal target_account_id");
                    }
                    if (sqlite3_column_type(stmt.get(), 7) != SQLITE_NULL) {
                        entry.subjectCard = domain::CardNumber::parse(
                            extractString(stmt.get(), 7, "validateDatabase journal subject_card")
                        );
                    }
                    entry.amount = domain::Money::ofKopiykas(
                        extractInt64(stmt.get(), 8, "validateDatabase journal amount_kop")
                    );

                    const auto denominations = domain::kAllDenominations;
                    domain::BanknoteSet banknotes;
                    for (std::size_t i = 0; i < denominations.size(); ++i) {
                        const std::int64_t count = extractInt64(
                            stmt.get(),
                            static_cast<int>(9 + i),
                            "validateDatabase journal cash"
                        );
                        if (count < 0 || count > max_uint32) {
                            throw StorageError("validateDatabase journal", SQLITE_RANGE, "Banknote count out of range");
                        }
                        banknotes = banknotes.with(denominations[i], static_cast<std::uint32_t>(count));
                    }
                    entry.cash = banknotes;

                    if (sqlite3_column_type(stmt.get(), 14) != SQLITE_NULL) {
                        entry.payeeId = extractInt64(stmt.get(), 14, "validateDatabase journal payee_id");
                    }
                    if (sqlite3_column_type(stmt.get(), 15) != SQLITE_NULL) {
                        entry.requisite = extractString(stmt.get(), 15, "validateDatabase journal requisite");
                    }
                } catch (const StorageError&) {
                    throw;
                } catch (...) {
                    throw StorageError("validateDatabase journal", SQLITE_CORRUPT, "Corrupted journal fields");
                }

                if (!domain::isValidJournalEntry(entry)) {
                    throw StorageError("validateDatabase journal", SQLITE_CORRUPT, "Inconsistent journal entry");
                }
            }

            if (rc != SQLITE_DONE) {
                throw StorageError("validateDatabase journal step", rc, sqlite3_errmsg(handle));
            }
        }
    }

} // namespace atm::infrastructure::sql
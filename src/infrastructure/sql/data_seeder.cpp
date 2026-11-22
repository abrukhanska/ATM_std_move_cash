#include "data_seeder.hpp"
#include "domain/transaction.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "infrastructure/sql/stmt.hpp"

#include <sqlite3.h>

namespace atm::infrastructure::sql {

	namespace {

		bool isEmpty(sqlite3* handle, const std::string& table) {
			const std::string sql = "SELECT COUNT(*) FROM " + table + ";";

			Stmt stmt = prepare(handle, sql.data(), "isEmpty prepare");

			int rc = sqlite3_step(stmt.get());
			if(rc != SQLITE_ROW) {
				throw StorageError("isEmpty step", rc, sqlite3_errmsg(handle));
			}

			return sqlite3_column_int64(stmt.get(), 0) == 0;
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

    void validateDatabase(Database &db)
    {
		sqlite3* handle = db.handle();
        const auto max_uint32 = static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());

        {
            const char* sql = "SELECT denomination, count FROM banknotes;";
            Stmt stmt = prepare(handle, sql, "validateDatabase banknotes prepare");

            std::array<bool, domain::kDenominationCount> found_denominations{};
            std::size_t row_count = 0;
            int rc = SQLITE_OK;

            while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                std::int64_t denomination = sqlite3_column_int64(stmt.get(), 0);
                std::int64_t count = sqlite3_column_int64(stmt.get(), 1);

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
                if (!matched) throw StorageError("validateDatabase banknotes", SQLITE_CORRUPT, "Unknown banknote denomination");
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
                SELECT c.card_number, c.kind, c.status, c.failed_attempts, COUNT(a.account_id) AS total_accounts, COALESCE(SUM(CASE WHEN a.is_primary = 1 THEN 1 ELSE 0 END), 0) AS primary_accounts
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
                const auto* card_number_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
                int card_number_length = sqlite3_column_bytes(stmt.get(), 0);
                const auto* kind_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
                const auto* status_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
                int failed_attempts = sqlite3_column_int(stmt.get(), 3);
                int total_accounts = sqlite3_column_int(stmt.get(), 4);
                int primary_accounts = sqlite3_column_int(stmt.get(), 5);

                std::string_view card_number(card_number_ptr ? card_number_ptr : "", static_cast<std::size_t>(card_number_length));
                try {
                    static_cast<void>(domain::CardNumber::parse(card_number));
                } catch (const std::invalid_argument&) {
                    throw StorageError("validateDatabase cards", SQLITE_CORRUPT, "Invalid card number format");
                }

                std::string_view kind = kind_ptr ? kind_ptr : "";
                std::string_view status = status_ptr ? status_ptr : "";

                if (kind == client) {
                    bool valid_active = (status == active && failed_attempts >= 0 && failed_attempts <= 2);
                    bool valid_retained = (status == retained && failed_attempts == 3);
                    if (!valid_active && !valid_retained) {
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
                std::int64_t id = sqlite3_column_int64(stmt.get(), 0);
                std::int64_t balance = sqlite3_column_int64(stmt.get(), 1);
                int is_primary = sqlite3_column_int(stmt.get(), 2);
                bool owner = (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL);

                if (id <= 0 || balance < 0 || (is_primary != 0 && is_primary != 1) || !owner) {
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
                std::int64_t id = sqlite3_column_int64(stmt.get(), 0);
                if (id <= 0) {
                    throw StorageError("validateDatabase journal", SQLITE_CORRUPT, "Invalid journal_id");
                }

                const auto* local_day_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
                int local_day_length = sqlite3_column_bytes(stmt.get(), 2);
                const auto* kind_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
                int kind_length = sqlite3_column_bytes(stmt.get(), 3);
                const auto* actor_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
                int actor_length = sqlite3_column_bytes(stmt.get(), 4);

                auto optional_kind = domain::toEntryKind(
                    std::string_view(kind_ptr ? kind_ptr : "", static_cast<std::size_t>(kind_length))
                );
                if (!optional_kind.has_value()) {
                    throw StorageError("validateDatabase journal", SQLITE_CORRUPT, "Unknown journal kind");
                }

                domain::JournalEntry entry;
                entry.id = id;
                entry.atUtc = sqlite3_column_int64(stmt.get(), 1);
                entry.localDay = std::string(local_day_ptr ? local_day_ptr : "", static_cast<std::size_t>(local_day_length));
                entry.kind = *optional_kind;

                try {
                    entry.actor = domain::CardNumber::parse(std::string_view(actor_ptr ? actor_ptr : "", static_cast<std::size_t>(actor_length)));
                    if (sqlite3_column_type(stmt.get(), 5) != SQLITE_NULL) {
                        entry.account = sqlite3_column_int64(stmt.get(), 5);
                    }
                    if (sqlite3_column_type(stmt.get(), 6) != SQLITE_NULL) {
                        entry.target = sqlite3_column_int64(stmt.get(), 6);
                    }
                    if (sqlite3_column_type(stmt.get(), 7) != SQLITE_NULL) {
                        const auto* subject_card_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 7));
                        int subject_card_length = sqlite3_column_bytes(stmt.get(), 7);
                        entry.subjectCard = domain::CardNumber::parse(
                            std::string_view(subject_card_ptr ? subject_card_ptr : "", static_cast<std::size_t>(subject_card_length))
                        );
                    }
                    entry.amount = domain::Money::ofKopiykas(sqlite3_column_int64(stmt.get(), 8));

                    const std::array<domain::Denomination, domain::kDenominationCount> denominations = domain::kAllDenominations;
                    domain::BanknoteSet banknotes;
                    for (std::size_t i = 0; i < denominations.size(); ++i) {
                        std::int64_t count = sqlite3_column_int64(stmt.get(), static_cast<int>(9 + i));
                        if (count < 0 || count > max_uint32) {
                            throw StorageError("validateDatabase journal", SQLITE_RANGE, "Banknote count out of range");
                        }
                        banknotes = banknotes.with(denominations[i], static_cast<std::uint32_t>(count));
                    }
                    entry.cash = banknotes;

                    if (sqlite3_column_type(stmt.get(), 14) != SQLITE_NULL) {
                        entry.payeeId = sqlite3_column_int64(stmt.get(), 14);
                    }
                    if (sqlite3_column_type(stmt.get(), 15) != SQLITE_NULL) {
                        const auto* requisite_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 15));
                        int requisite_length = sqlite3_column_bytes(stmt.get(), 15);
                        if (requisite_ptr && requisite_length > 0) {
                            entry.requisite.assign(requisite_ptr, static_cast<std::size_t>(requisite_length));
                        }
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

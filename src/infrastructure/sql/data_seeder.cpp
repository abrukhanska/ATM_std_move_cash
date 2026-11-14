#include "data_seeder.hpp"
#include "domain/transaction.hpp"
#include "infrastructure/sql/storage_error.hpp"

#include <sqlite3.h>

namespace atm::infrastructure::sql {

	struct StmtFinalizer {
			void operator()(sqlite3_stmt* stmt) const noexcept {
				if (stmt) {
					sqlite3_finalize(stmt);
				}
			}
		};
	using Stmt = std::unique_ptr<sqlite3_stmt, StmtFinalizer>;

	namespace {

		bool isEmpty(sqlite3* handle, const std::string& table) {
			std::string sql = "SELECT COUNT(*) FROM " + table + ";";

			sqlite3_stmt* stmt_ptr = nullptr;
			int rc = sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt_ptr, nullptr);
			if(rc != SQLITE_OK) {
				throw StorageError("isEmpty prepare", rc, sqlite3_errmsg(handle));
			}

			Stmt stmt(stmt_ptr);

			rc = sqlite3_step(stmt.get());
			if(rc != SQLITE_ROW) {
				throw StorageError("isEmpty step", rc, sqlite3_errmsg(handle));
			}

			return sqlite3_column_int(stmt_ptr, 0) == 0;
		}

	} // namespace

	void seedDemoDataIfEmpty(Database& db, const std::vector<domain::Card>& cards, const std::vector<domain::Account>& accounts) {
		sqlite3* handle = db.handle();

		if(!isEmpty(handle, "cards") || !isEmpty(handle, "accounts") || !isEmpty(handle, "banknotes") || !isEmpty(handle, "journal")) {
			return;
		}

		domain::Transaction transaction(db);

		{
			const char* banknotes_sql = "INSERT INTO banknotes (denomination, count) VALUES (?, 0);";

			sqlite3_stmt* stmt_ptr = nullptr;
			int rc = sqlite3_prepare_v2(handle, banknotes_sql, -1, &stmt_ptr, nullptr);
			if (rc != SQLITE_OK) {
				throw StorageError("seedDemoDataIfEmpty banknotes prepare", rc, sqlite3_errmsg(handle));
			}

			Stmt stmt(stmt_ptr);

			for (domain::Denomination denomination : domain::kAllDenominations) {
				sqlite3_bind_int64(stmt.get(), 1, domain::denominationUah(denomination));

				rc = sqlite3_step(stmt.get());
				if (rc != SQLITE_DONE) {
					throw StorageError("seedDemoDataIfEmpty banknotes step", rc, sqlite3_errmsg(handle));
				}
				sqlite3_reset(stmt.get());
			}
		}

		{
            const char* cards_sql = "INSERT INTO cards (card_number, holder_name, pin_hash, kind, status, failed_attempts) VALUES (?, ?, ?, ?, ?, ?);";

            sqlite3_stmt* stmt_ptr = nullptr;
			int rc = sqlite3_prepare_v2(handle, cards_sql, -1, &stmt_ptr, nullptr);
			if (rc != SQLITE_OK) {
				throw StorageError("seedDemoDataIfEmpty cards prepare", rc, sqlite3_errmsg(handle));
			}
			Stmt stmt(stmt_ptr);

            for (const domain::Card& card : cards) {
                std::string card_number = card.number().str();
                std::string_view kind = domain::toString(card.kind());
                std::string_view status = domain::toString(card.status());

                sqlite3_bind_text(stmt.get(), 1, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt.get(), 2, card.holderName().c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt.get(), 3, card.pinHash().c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt.get(), 4, kind.data(), static_cast<int>(kind.size()), SQLITE_STATIC);
                sqlite3_bind_text(stmt.get(), 5, status.data(), static_cast<int>(status.size()), SQLITE_STATIC);
                sqlite3_bind_int(stmt.get(), 6, card.failedAttempts());

                rc = sqlite3_step(stmt.get());
                if (rc != SQLITE_DONE) {
                    throw StorageError("seedDemoDataIfEmpty cards step", rc, sqlite3_errmsg(handle));
                }
                sqlite3_reset(stmt.get());
            }
        }

		{
            const char* accounts_sql = "INSERT INTO accounts (account_id, card_number, balance_kop, is_primary) VALUES (?, ?, ?, ?);";

            sqlite3_stmt* stmt_ptr = nullptr;
			int rc = sqlite3_prepare_v2(handle, accounts_sql, -1, &stmt_ptr, nullptr);
			if (rc != SQLITE_OK) {
				throw StorageError("seedDemoDataIfEmpty accounts prepare", rc, sqlite3_errmsg(handle));
			}
			Stmt stmt(stmt_ptr);

            for (const domain::Account& account : accounts) {
                std::string card_number = account.owner().str();

                sqlite3_bind_int64(stmt.get(), 1, account.id());
                sqlite3_bind_text(stmt.get(), 2, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt.get(), 3, account.balance().kopiykas());
                sqlite3_bind_int(stmt.get(), 4, account.isPrimary() ? 1 : 0);

                rc = sqlite3_step(stmt.get());
                if (rc != SQLITE_DONE) {
                    throw StorageError("seedDemoDataIfEmpty accounts step", rc, sqlite3_errmsg(handle));
                }
                sqlite3_reset(stmt.get());
            }
        }

		transaction.commit();
	}

    void infrastructure::sql::validateDatabase(Database &db)
    {
		sqlite3* handle = db.handle();
        const auto max_uint32 = static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());

        {
            const char* sql = "SELECT denomination, count FROM banknotes;";

            sqlite3_stmt* stmt_ptr = nullptr;
            int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
            if (rc != SQLITE_OK) {
                throw StorageError("validateDatabase banknotes prepare", rc, sqlite3_errmsg(handle));
            }
            Stmt stmt(stmt_ptr);

            std::array<bool, domain::kDenominationCount> found_denominations{};
            std::size_t row_count = 0;

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

            sqlite3_stmt* stmt_ptr = nullptr;
            int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
            if (rc != SQLITE_OK) {
                throw StorageError("validateDatabase cards prepare", rc, sqlite3_errmsg(handle));
            }
            Stmt stmt(stmt_ptr);

            const std::string_view client = domain::toString(domain::CardKind::Client);
            const std::string_view service = domain::toString(domain::CardKind::Service);
            const std::string_view active = domain::toString(domain::CardStatus::Active);
            const std::string_view retained = domain::toString(domain::CardStatus::Retained);

            while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                const auto* card_number_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
                int card_number_length = sqlite3_column_bytes(stmt.get(), 0);
                const auto* kind_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
                const auto* status_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
                int failed_attempts = sqlite3_column_int(stmt.get(), 3);
                int total_accounts = sqlite3_column_int(stmt.get(), 4);
                int primary_accounts = sqlite3_column_int(stmt.get(), 5);

                std::string_view card_number_sv(card_number_ptr ? card_number_ptr : "", static_cast<std::size_t>(card_number_length));
                try {
                    static_cast<void>(domain::CardNumber::parse(card_number_sv));
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

            sqlite3_stmt* stmt_ptr = nullptr;
            int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
            if (rc != SQLITE_OK) {
                throw StorageError("validateDatabase accounts prepare", rc, sqlite3_errmsg(handle));
            }
            Stmt stmt(stmt_ptr);

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
                SELECT COUNT(*) FROM journal
                WHERE journal_id <= 0
                   OR amount_kop < 0
                   OR cash_100 < 0 OR cash_100 > ?
                   OR cash_200 < 0 OR cash_200 > ?
                   OR cash_500 < 0 OR cash_500 > ?
                   OR cash_1000 < 0 OR cash_1000 > ?
                   OR cash_2000 < 0 OR cash_2000 > ?;
            )";

            sqlite3_stmt* stmt_ptr = nullptr;
            int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
            if (rc != SQLITE_OK) {
                throw StorageError("validateDatabase journal prepare", rc, sqlite3_errmsg(handle));
            }
            Stmt stmt(stmt_ptr);

            for (int i = 1; i <= 5; ++i) {
                sqlite3_bind_int64(stmt.get(), i, max_uint32);
            }

            if (sqlite3_step(stmt.get()) != SQLITE_ROW || sqlite3_column_int(stmt.get(), 0) != 0) {
                throw StorageError("validateDatabase journal", SQLITE_CORRUPT, "Corrupted journal records");
            }
        }
    }

} // namespace atm::infrastructure::sql

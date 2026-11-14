#include "infrastructure/sql/journal_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "domain/domain_error.hpp"

#include <sqlite3.h>
#include <cstdint>
#include <limits>

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
        std::string extractString(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
                return {};
            }
            const auto* text_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
            int length = sqlite3_column_bytes(stmt, column);
            if (!text_ptr || length <= 0) {
                return {};
            }
            return std::string(text_ptr, static_cast<std::size_t>(length));
        }

        std::optional<domain::AccountId> extractAccountId(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
                return std::nullopt;
            }
            domain::AccountId id = sqlite3_column_int64(stmt, column);
            if (id <= 0) {
                throw StorageError("JournalRepository::extractAccountId", SQLITE_CORRUPT, "Invalid account_id");
            }
            return id;
        }

        std::optional<std::int64_t> extractPayeeId(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
                return std::nullopt;
            }
            return sqlite3_column_int64(stmt, column);
        }

        domain::CardNumber extractCard(sqlite3_stmt* stmt, int column) {
            const auto* card_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
            if (!card_ptr) {
                throw StorageError("JournalRepository::extractCard", SQLITE_CORRUPT, "Null card_number");
            }
            int length = sqlite3_column_bytes(stmt, column);
            try {
                return domain::CardNumber::parse(
                    std::string_view(card_ptr, static_cast<std::size_t>(length))
                );
            } catch (const std::invalid_argument&) {
                throw StorageError("JournalRepository::extractCard", SQLITE_CORRUPT, "Invalid card_number");
            }
        }

        std::optional<domain::CardNumber> extractOptionalCard(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
                return std::nullopt;
            }
            return extractCard(stmt, column);
        }

        std::uint32_t extractCashCount(sqlite3_stmt* stmt, int column) {
            std::int64_t count = sqlite3_column_int64(stmt, column);
            if (count < 0 || count > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
                throw StorageError("JournalRepository::extractCashCount", SQLITE_RANGE, "Banknote count out of range");
            }
            return static_cast<std::uint32_t>(count);
        }

        domain::JournalEntry extractEntry(sqlite3_stmt* stmt) {
            domain::JournalEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.atUtc = sqlite3_column_int64(stmt, 1);
            entry.localDay = extractString(stmt, 2);

            std::string kind_str = extractString(stmt, 3);
            auto optional_kind = domain::toEntryKind(kind_str);
            if (!optional_kind.has_value()) {
                throw StorageError("JournalRepository::extractEntry", SQLITE_CORRUPT, "Invalid EntryKind: " + kind_str);
            }
            entry.kind = optional_kind.value();

            entry.actor = extractCard(stmt, 4);
            entry.account = extractAccountId(stmt, 5);
            entry.target = extractAccountId(stmt, 6);
            entry.subjectCard = extractOptionalCard(stmt, 7);

            entry.amount = [&]() {
                try {
                    return domain::Money::ofKopiykas(sqlite3_column_int64(stmt, 8));
                } catch (const domain::DomainError&) {
                    throw StorageError("JournalRepository::extractEntry", SQLITE_CORRUPT, "Invalid amount_kop");
                }
            }();

            std::uint32_t uah100  = extractCashCount(stmt, 9);
            std::uint32_t uah200  = extractCashCount(stmt, 10);
            std::uint32_t uah500  = extractCashCount(stmt, 11);
            std::uint32_t uah1000 = extractCashCount(stmt, 12);
            std::uint32_t uah2000 = extractCashCount(stmt, 13);

            entry.cash = domain::BanknoteSet()
                .with(domain::Denomination::UAH100, uah100)
                .with(domain::Denomination::UAH200, uah200)
                .with(domain::Denomination::UAH500, uah500)
                .with(domain::Denomination::UAH1000, uah1000)
                .with(domain::Denomination::UAH2000, uah2000);

            auto optional_payee = extractPayeeId(stmt, 14);
            entry.payeeId = optional_payee.value_or(0);

            entry.requisite = extractString(stmt, 15);

            return entry;
        }

        int bindInt(sqlite3_stmt* stmt, int column, std::optional<std::int64_t> val) {
            return val ? sqlite3_bind_int64(stmt, column, *val) : sqlite3_bind_null(stmt, column);
        }

        int bindString(sqlite3_stmt* stmt, int column, const std::optional<std::string>& val) {
            if (val && !val->empty()) return sqlite3_bind_text(stmt, column, val->data(), static_cast<int>(val->size()), SQLITE_TRANSIENT);
            return sqlite3_bind_null(stmt, column);
        }
    }

    domain::JournalEntry JournalRepository::append(const domain::JournalEntry &entry)
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "INSERT INTO journal "
                          "(at_utc, local_day, kind, actor_card, account_id, target_account_id, subject_card, amount_kop, "
                          "cash_100, cash_200, cash_500, cash_1000, cash_2000, "
                          "payee_id, requisite) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("JournalRepository::append prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);

        std::string_view kind_sv = domain::toString(entry.kind);
        std::string actor = entry.actor.str();
        std::optional<std::string> subject_card = entry.subjectCard ? std::make_optional(entry.subjectCard->str()) : std::nullopt;

        auto uah100  = static_cast<std::int64_t>(entry.cash.count(domain::Denomination::UAH100));
        auto uah200  = static_cast<std::int64_t>(entry.cash.count(domain::Denomination::UAH200));
        auto uah500  = static_cast<std::int64_t>(entry.cash.count(domain::Denomination::UAH500));
        auto uah1000 = static_cast<std::int64_t>(entry.cash.count(domain::Denomination::UAH1000));
        auto uah2000 = static_cast<std::int64_t>(entry.cash.count(domain::Denomination::UAH2000));

        if (sqlite3_bind_int64(stmt.get(), 1, entry.atUtc) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 2, entry.localDay.data(), static_cast<int>(entry.localDay.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 3, kind_sv.data(), static_cast<int>(kind_sv.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 4, actor.data(), static_cast<int>(actor.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            bindInt(stmt.get(), 5, entry.account ? std::make_optional(static_cast<std::int64_t>(*entry.account)) : std::nullopt) != SQLITE_OK ||
            bindInt(stmt.get(), 6, entry.target ? std::make_optional(static_cast<std::int64_t>(*entry.target)) : std::nullopt) != SQLITE_OK ||
            bindString(stmt.get(), 7, subject_card) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 8, entry.amount.kopiykas()) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 9, uah100) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 10, uah200) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 11, uah500) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 12, uah1000) != SQLITE_OK ||
            sqlite3_bind_int64(stmt.get(), 13, uah2000) != SQLITE_OK ||
            bindInt(stmt.get(), 14, entry.payeeId.value_or(0) > 0 ? entry.payeeId : std::nullopt) != SQLITE_OK ||
            bindString(stmt.get(), 15, entry.requisite.empty() ? std::nullopt : std::make_optional(entry.requisite)) != SQLITE_OK) {
            throw StorageError("JournalRepository::append bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) {
            throw StorageError("JournalRepository::append step", rc, sqlite3_errmsg(handle));
        }

        domain::JournalEntry saved = entry;
        saved.id = sqlite3_last_insert_rowid(handle);
        return saved;
    }

    std::vector<domain::JournalEntry> JournalRepository::last(std::size_t count) const
    {
        if (count > static_cast<std::size_t>(std::numeric_limits<sqlite3_int64>::max())) {
            throw StorageError("JournalRepository::last", SQLITE_RANGE, "Limit count out of range");
        }

        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT journal_id, at_utc, local_day, kind, actor_card, account_id, "
                          "target_account_id, subject_card, amount_kop, "
                          "cash_100, cash_200, cash_500, cash_1000, cash_2000, "
                          "payee_id, requisite "
                          "FROM journal ORDER BY journal_id DESC LIMIT ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("JournalRepository::last prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);

        if (sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(count)) != SQLITE_OK) {
            throw StorageError("JournalRepository::last bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        std::vector<domain::JournalEntry> entries;
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            entries.push_back(extractEntry(stmt.get()));
        }
        if (rc != SQLITE_DONE) {
            throw StorageError("JournalRepository::last step", rc, sqlite3_errmsg(handle));
        }

        return entries;
    }

    domain::Money JournalRepository::withdrawnOn(const domain::CardNumber &number, const std::string &day) const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;
        const char* sql = "SELECT SUM(amount_kop) FROM journal WHERE actor_card = ? AND local_day = ? AND kind = ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("JournalRepository::withdrawnOn prepare", rc, sqlite3_errmsg(handle));
        }
        Stmt stmt(stmt_ptr);

        std::string card_number = number.str();
        std::string_view kind_withdraw = domain::toString(domain::EntryKind::Withdrawal);

        if (sqlite3_bind_text(stmt.get(), 1, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 2, day.data(), static_cast<int>(day.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 3, kind_withdraw.data(), static_cast<int>(kind_withdraw.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::withdrawnOn bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
                return domain::Money::ofKopiykas(0);
            }
            try {
                return domain::Money::ofKopiykas(sqlite3_column_int64(stmt.get(), 0));
            } catch (const domain::DomainError&) {
                throw StorageError("JournalRepository::withdrawnOn", SQLITE_CORRUPT, "Invalid SUM(amount_kop)");
            }
        }
        if (rc != SQLITE_DONE) {
            throw StorageError("JournalRepository::withdrawnOn step", rc, sqlite3_errmsg(handle));
        }

        return domain::Money::ofKopiykas(0);
    }

    domain::Money JournalRepository::sumAmount(domain::EntryKind kind) const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT SUM(amount_kop) FROM journal WHERE kind = ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("JournalRepository::sumAmount prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);
        std::string_view kind_sv = domain::toString(kind);

        if (sqlite3_bind_text(stmt.get(), 1, kind_sv.data(), static_cast<int>(kind_sv.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::sumAmount bind kind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
                return domain::Money::ofKopiykas(0);
            }
            try {
                return domain::Money::ofKopiykas(sqlite3_column_int64(stmt.get(), 0));
            } catch (const domain::DomainError&) {
                throw StorageError("JournalRepository::sumAmount", SQLITE_CORRUPT, "Invalid SUM(amount_kop)");
            }
        }
        if (rc != SQLITE_DONE) {
            throw StorageError("JournalRepository::sumAmount step", rc, sqlite3_errmsg(handle));
        }

        return domain::Money::ofKopiykas(0);
    }

} // namespace atm::infrastructure::sql
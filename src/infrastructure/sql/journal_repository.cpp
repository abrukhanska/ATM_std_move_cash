#include "infrastructure/sql/journal_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"
#include "domain/enums.hpp"

#include <sqlite3.h>

namespace atm::infrastructure::sql {

    struct SqliteStmtDeleter {
        void operator()(sqlite3_stmt* stmt) const noexcept {
            if (stmt) {
                sqlite3_finalize(stmt);
            }
        }
    };
    using Stmt = std::unique_ptr<sqlite3_stmt, SqliteStmtDeleter>;

    namespace {
        std::optional<domain::AccountId> extractOptionalAccountId(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) return std::nullopt;
            return static_cast<domain::AccountId>(sqlite3_column_int64(stmt, column));
        }

        std::optional<std::int64_t> extractOptionalPayeeId(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) return std::nullopt;
            return sqlite3_column_int64(stmt, column);
        }

        std::optional<domain::CardNumber> extractOptionalCard(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) return std::nullopt;
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
            return text ? std::make_optional(domain::CardNumber::parse(text)) : std::nullopt;
        }

        std::string extractString(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) return "";
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
            return text ? std::string(text) : "";
        }

        domain::JournalEntry extractEntry(sqlite3_stmt* stmt) {
            domain::JournalEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.atUtc = sqlite3_column_int64(stmt, 1);
            entry.localDay = extractString(stmt, 2);

            std::string kind_str = extractString (stmt, 3);
            auto opt_kind = domain::toEntryKind(kind_str);
            if (!opt_kind.has_value()) {
                throw std::runtime_error("Database corruption: invalid EntryKind '" + kind_str + "' for journal entry ID " + std::to_string(entry.id));
            }
            entry.kind = opt_kind.value();

            entry.actor = domain::CardNumber::parse(extractString(stmt, 4));
            entry.account = extractOptionalAccountId(stmt, 5);
            entry.target = extractOptionalAccountId(stmt, 6);
            entry.subjectCard = extractOptionalCard(stmt, 7);
            entry.amount = domain::Money::ofKopiykas(sqlite3_column_int64(stmt, 8));
           
            std::uint32_t uah100  = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 9));
            std::uint32_t uah200  = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 10));
            std::uint32_t uah500  = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 11));
            std::uint32_t uah1000 = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 12));
            std::uint32_t uah2000 = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 13));

            entry.cash = domain::BanknoteSet()
                .with(domain::Denomination::UAH100, uah100)
                .with(domain::Denomination::UAH200, uah200)
                .with(domain::Denomination::UAH500, uah500)
                .with(domain::Denomination::UAH1000, uah1000)
                .with(domain::Denomination::UAH2000, uah2000);

            auto opt_payee = extractOptionalPayeeId(stmt, 14);
            entry.payeeId = opt_payee.has_value() ? opt_payee.value() : 0;

            entry.requisite = extractString(stmt, 15);
            
            return entry;
        }
        
        void bindOptionalInt64(sqlite3_stmt* stmt, int col, std::optional<std::int64_t> val) {
            if (val) sqlite3_bind_int64(stmt, col, *val);
            else sqlite3_bind_null(stmt, col);
        }

        void bindOptionalString(sqlite3_stmt* stmt, int col, const std::optional<std::string>& val) {
            if (val && !val->empty()) sqlite3_bind_text(stmt, col, val->c_str(), -1, SQLITE_TRANSIENT);
            else sqlite3_bind_null(stmt, col);
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
            throw StorageError("JournalRepository::append (prepare)", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);

        std::string local_day = entry.localDay;
        std::string kind = std::string(domain::toString(entry.kind));
        std::string actor = entry.actor.str();
        std::optional<std::string> subject_card = entry.subjectCard ? std::make_optional(entry.subjectCard->str()) : std::nullopt;
        std::string requisite = entry.requisite;

        int uah100  = static_cast<int>(entry.cash.count(domain::Denomination::UAH100));
        int uah200  = static_cast<int>(entry.cash.count(domain::Denomination::UAH200));
        int uah500  = static_cast<int>(entry.cash.count(domain::Denomination::UAH500));
        int uah1000 = static_cast<int>(entry.cash.count(domain::Denomination::UAH1000));
        int uah2000 = static_cast<int>(entry.cash.count(domain::Denomination::UAH2000));

        if (sqlite3_bind_int64(stmt.get(), 1, entry.atUtc) != SQLITE_OK) {
            throw StorageError("JournalRepository::append bind at_utc", SQLITE_ERROR, sqlite3_errmsg(handle));
        }
        if (sqlite3_bind_text(stmt.get(), 2, local_day.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::append bind local_day", SQLITE_ERROR, sqlite3_errmsg(handle));
        }
        if (sqlite3_bind_text(stmt.get(), 3, kind.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::append bind kind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }
        if (sqlite3_bind_text(stmt.get(), 4, actor.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::append bind actor_card", SQLITE_ERROR, sqlite3_errmsg(handle));
        }
        
        bindOptionalInt64(stmt.get(), 5, entry.account ? std::make_optional(static_cast<std::int64_t>(*entry.account)) : std::nullopt);
        bindOptionalInt64(stmt.get(), 6, entry.target ? std::make_optional(static_cast<std::int64_t>(*entry.target)) : std::nullopt);
        bindOptionalString(stmt.get(), 7, subject_card);

        if (sqlite3_bind_int64(stmt.get(), 8, entry.amount.kopiykas()) != SQLITE_OK) {
            throw StorageError("JournalRepository::append bind amount", SQLITE_ERROR, sqlite3_errmsg(handle));
        }
        
        if (sqlite3_bind_int(stmt.get(), 9,  uah100) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(), 10, uah200) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(), 11, uah500) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(), 12, uah1000) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(), 13, uah2000) != SQLITE_OK) {
            throw StorageError("JournalRepository::append bind cash", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        bindOptionalInt64(stmt.get(), 14, entry.payeeId);
        
        if (!requisite.empty()) {
            sqlite3_bind_text(stmt.get(), 15, requisite.c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt.get(), 15);
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
        if (rc != SQLITE_DONE) throw StorageError("JournalRepository::last step", rc, sqlite3_errmsg(handle));

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
        std::string kind_withdraw = "Withdrawal";

        if (sqlite3_bind_text(stmt.get(), 1, card_number.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::withdrawnOn bind actor_card", SQLITE_ERROR, sqlite3_errmsg(handle));
        }
        if (sqlite3_bind_text(stmt.get(), 2, day.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::withdrawnOn bind local_day", SQLITE_ERROR, sqlite3_errmsg(handle));
        }
        if (sqlite3_bind_text(stmt.get(), 3, kind_withdraw.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::withdrawnOn bind kind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
                return domain::Money::ofKopiykas(0);
            }
            return domain::Money::ofKopiykas(sqlite3_column_int64(stmt.get(), 0));
        }
        if (rc != SQLITE_DONE) throw StorageError("JournalRepository::withdrawnOn step", rc, sqlite3_errmsg(handle));

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

        std::string kind_str = std::string(domain::toString(kind));

        if (sqlite3_bind_text(stmt.get(), 1, kind_str.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("JournalRepository::sumAmount bind kind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
                return domain::Money::ofKopiykas(0);
            }
            return domain::Money::ofKopiykas(sqlite3_column_int64(stmt.get(), 0));
        }
        if (rc != SQLITE_DONE) throw StorageError("JournalRepository::sumAmount step", rc, sqlite3_errmsg(handle));

        return domain::Money::ofKopiykas(0);
    }
    
} // namespace atm::infrastructure::sql
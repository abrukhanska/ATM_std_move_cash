#include "infrastructure/sql/card_repository.hpp"
#include "infrastructure/sql/database.hpp"
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
        std::string extractString(sqlite3_stmt* stmt, int column) {
           const auto* text_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
            int length = sqlite3_column_bytes(stmt, column);
            if (!text_ptr || length <= 0) {
                return {};
            }
            return std::string(text_ptr, static_cast<std::size_t>(length));
        }

        domain::Card extractCard(sqlite3_stmt* stmt) {
            domain::CardNumber parsed_card_number = [&]() {
                const auto* card_number_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (!card_number_ptr) {
                    throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, "Null card_number");
                }
                int card_number_length = sqlite3_column_bytes(stmt, 0);
                try {
                    return domain::CardNumber::parse(std::string_view(card_number_ptr, static_cast<std::size_t>(card_number_length)));
                } catch (const std::invalid_argument&) {
                    throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, "Invalid card_number");
                }
            }();

            std::string holder_name = extractString(stmt, 1);
            std::string pin_hash = extractString(stmt, 2);

            std::string kind_str = extractString(stmt, 3);
            auto opt_kind = domain::toCardKind(kind_str);
            if (!opt_kind.has_value()) {
                throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, "Invalid CardKind: " + kind_str);
            }
            domain::CardKind kind = opt_kind.value();

            std::string status_str = extractString(stmt, 4);
            auto opt_status = domain::toCardStatus(status_str);
            if (!opt_status.has_value()) {
                throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, "Invalid CardStatus: " + status_str);
            }
            domain::CardStatus status = opt_status.value();

            std::int64_t failed_attempts_int64 = sqlite3_column_int64(stmt, 5);
            if (failed_attempts_int64 < 0 || failed_attempts_int64 > 3) {
                throw StorageError("CardRepository::extractCard", SQLITE_RANGE, "failed_attempts out of range");
            }
            std::uint8_t failed_attempts = static_cast<std::uint8_t>(failed_attempts_int64);

            return domain::Card(
                parsed_card_number,
                holder_name,
                pin_hash,
                kind,
                status,
                failed_attempts
            );
        }
    }

    std::optional<domain::Card> CardRepository::find(const domain::CardNumber &number) const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT card_number, holder_name, pin_hash, kind, status, failed_attempts FROM cards WHERE card_number = ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("CardRepository::find prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);
        std::string card_number = number.str();

        if (sqlite3_bind_text(stmt.get(), 1, card_number.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("CardRepository::find bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) return extractCard(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("CardRepository::find step", rc, sqlite3_errmsg(handle));

        return std::nullopt;
    }

    std::vector<domain::Card> CardRepository::listRetained() const
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "SELECT card_number, holder_name, pin_hash, kind, status, failed_attempts FROM cards WHERE status = ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("CardRepository::listRetained prepare", rc, sqlite3_errmsg(handle));
        }

        Stmt stmt(stmt_ptr);
        std::string status_retained = "Retained";

        if (sqlite3_bind_text(stmt.get(), 1, status_retained.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("CardRepository::listRetained bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        std::vector<domain::Card> cards;
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            cards.push_back(extractCard(stmt.get()));
        }
        if (rc != SQLITE_DONE) throw StorageError("CardRepository::listRetained step", rc, sqlite3_errmsg(handle));

        return cards;
    }


    void CardRepository::save(const domain::Card &card)
    {
        sqlite3* handle = db_.handle();
        sqlite3_stmt* stmt_ptr = nullptr;

        const char* sql = "UPDATE cards SET holder_name = ?, pin_hash = ?, kind = ?, status = ?, failed_attempts = ? WHERE card_number = ?;";

        int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt_ptr, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError("CardRepository::save prepare", rc, sqlite3_errmsg(handle));
        }
        Stmt stmt(stmt_ptr);

        std::string holder_name = card.holderName();
        std::string pin_hash = card.pinHash();
        std::string_view kind_sv = atm::domain::toString(card.kind());
        std::string_view status_sv = atm::domain::toString(card.status());
        int failed_attempts = static_cast<int>(card.failedAttempts());
        std::string card_number = card.number().str();

        if (sqlite3_bind_text(stmt.get(), 1, holder_name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 2, pin_hash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 3, kind_sv.data(), static_cast<int>(kind_sv.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 4, status_sv.data(), static_cast<int>(status_sv.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(),  5, failed_attempts) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 6, card_number.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            
            throw StorageError("CardRepository::save bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("CardRepository::save step", rc, sqlite3_errmsg(handle));
    }

} // namespace atm::infrastructure::sql
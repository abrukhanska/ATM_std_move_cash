#include "infrastructure/sql/card_repository.hpp"
#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/storage_error.hpp"

#include <sqlite3.h>
#include "infrastructure/sql/stmt.hpp"

namespace atm::infrastructure::sql {

    namespace {
        std::string extractString(sqlite3_stmt* stmt, int column) {
            if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
                throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, std::string("Null ") + std::to_string(column));
            }
            const auto* text_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
            int text_length = sqlite3_column_bytes(stmt, column);
            if (!text_ptr || text_length <= 0) {
                throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, std::string("Empty ") + std::to_string(column));
            }
            return std::string(text_ptr, static_cast<std::size_t>(text_length));
        }

        domain::Card extractCard(sqlite3_stmt* stmt) {
            domain::CardNumber card_number = [&]() {
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
            auto optional_kind = domain::toCardKind(kind_str);
            if (!optional_kind.has_value()) {
                throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, "Invalid CardKind: " + kind_str);
            }
            domain::CardKind kind = optional_kind.value();

            std::string status_str = extractString(stmt, 4);
            auto optional_status = domain::toCardStatus(status_str);
            if (!optional_status.has_value()) {
                throw StorageError("CardRepository::extractCard", SQLITE_CORRUPT, "Invalid CardStatus: " + status_str);
            }
            domain::CardStatus status = optional_status.value();

            std::int64_t failed_attempts_int64 = sqlite3_column_int64(stmt, 5);
            if (failed_attempts_int64 < 0 || failed_attempts_int64 > 3) {
                throw StorageError("CardRepository::extractCard", SQLITE_RANGE, "failed_attempts out of range");
            }
            std::uint8_t failed_attempts = static_cast<std::uint8_t>(failed_attempts_int64);

            return domain::Card(
                card_number,
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
        const char* sql = "SELECT card_number, holder_name, pin_hash, kind, status, failed_attempts FROM cards WHERE card_number = ?;";
        Stmt stmt = prepare(handle, sql, "CardRepository::find prepare");

        std::string card_number = number.str();
        if (sqlite3_bind_text(stmt.get(), 1, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("CardRepository::find bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) return extractCard(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("CardRepository::find step", rc, sqlite3_errmsg(handle));

        return std::nullopt;
    }

    std::vector<domain::Card> CardRepository::listRetained() const
    {
        sqlite3* handle = db_.handle();
        const char* sql = "SELECT card_number, holder_name, pin_hash, kind, status, failed_attempts FROM cards WHERE status = ?;";
        Stmt stmt = prepare(handle, sql, "CardRepository::listRetained prepare");

        const auto status_retained = domain::toString(domain::CardStatus::Retained);
        if (sqlite3_bind_text(stmt.get(), 1, status_retained.data(), static_cast<int>(status_retained.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
            throw StorageError("CardRepository::listRetained bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        std::vector<domain::Card> cards;

        int rc = SQLITE_OK;
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            cards.push_back(extractCard(stmt.get()));
        }
        if (rc != SQLITE_DONE) throw StorageError("CardRepository::listRetained step", rc, sqlite3_errmsg(handle));

        return cards;
    }


    void CardRepository::save(const domain::Card &card)
    {
        sqlite3* handle = db_.handle();
        const char* sql = "UPDATE cards SET holder_name = ?, pin_hash = ?, kind = ?, status = ?, failed_attempts = ? WHERE card_number = ?;";
        Stmt stmt = prepare(handle, sql, "CardRepository::save prepare");

        std::string holder_name = card.holderName();
        std::string pin_hash = card.pinHash();
        std::string_view kind = atm::domain::toString(card.kind());
        std::string_view status = atm::domain::toString(card.status());
        int failed_attempts = static_cast<int>(card.failedAttempts());
        std::string card_number = card.number().str();

        if (sqlite3_bind_text(stmt.get(), 1, holder_name.data(), static_cast<int>(holder_name.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 2, pin_hash.data(), static_cast<int>(pin_hash.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 3, kind.data(), static_cast<int>(kind.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 4, status.data(), static_cast<int>(status.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_int(stmt.get(),  5, failed_attempts) != SQLITE_OK ||
            sqlite3_bind_text(stmt.get(), 6, card_number.data(), static_cast<int>(card_number.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
            
            throw StorageError("CardRepository::save bind", SQLITE_ERROR, sqlite3_errmsg(handle));
        }

        int rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) throw StorageError("CardRepository::save step", rc, sqlite3_errmsg(handle));
        if (sqlite3_changes(handle) != 1) throw StorageError("CardRepository::save step", SQLITE_NOTFOUND, "Card not found");
    }

} // namespace atm::infrastructure::sql
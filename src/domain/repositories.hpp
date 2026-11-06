#ifndef ATM_DOMAIN_REPOSITORIES_HPP
#define ATM_DOMAIN_REPOSITORIES_HPP

#include "account.hpp"
#include "card.hpp"
#include "card_number.hpp"
#include "journal_entry.hpp"
#include "banknote_set.hpp"

#include <optional>
#include <vector>

namespace atm::domain {

    class ICardRepository {
    public:
        virtual ~ICardRepository() noexcept = default;

        [[nodiscard]] virtual std::optional<Card> find(const CardNumber& number) const = 0;
        [[nodiscard]] virtual std::vector<Card> listRetained() const = 0;
        virtual void save(const Card& card) = 0;
    };

    class IAccountRepository {
    public:
        virtual ~IAccountRepository() noexcept = default;

        [[nodiscard]] virtual std::optional<Account> find(AccountId id) const = 0;
        [[nodiscard]] virtual std::optional<Account> findPrimary(const CardNumber& cardNumber) const = 0;
        [[nodiscard]] virtual std::vector<Account> listByCard(const CardNumber& cardNumber) const = 0;
        virtual void save(const Account& account) = 0;
    };

    class ICashRepository {
    public:
        virtual ~ICashRepository() noexcept = default;

        [[nodiscard]] virtual BanknoteSet load() const = 0;
        virtual void save(const BanknoteSet& banknotes) = 0;
    };

    class IJournalRepository {
    public:
        virtual ~IJournalRepository() noexcept = default;

        virtual JournalEntry append(const JournalEntry& entry) = 0;
        [[nodiscard]] virtual std::vector<JournalEntry> last(std::size_t count) const = 0;
        [[nodiscard]] virtual Money withdrawnOn(const CardNumber& cardNumber, const std::string& day) const = 0;
        [[nodiscard]] virtual Money sumAmount(EntryKind kind) const = 0;
    };

}  // namespace atm::domain

#endif  // ATM_DOMAIN_REPOSITORIES_HPP
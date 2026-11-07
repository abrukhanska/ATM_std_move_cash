#include "application/deposit_service.hpp"

#include <type_traits>
#include <utility>

#include "application/detail/access.hpp"
#include "domain/account.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/istorage.hpp"
#include "domain/transaction.hpp"

namespace atm::application {

    using domain::DomainError;
    using domain::ErrorReason;

    static_assert(std::is_nothrow_move_constructible_v<domain::JournalEntry>,
        "Returning a deposit result must not throw after commit");

    DepositService::DepositService(domain::IStorage& storage, domain::Clock clock)
        : storage_(storage), clock_(std::move(clock)) {
        if (!clock_) {
            throw DomainError(ErrorReason::InvalidData);
        }
    }

    domain::JournalEntry DepositService::deposit(
        const domain::CardNumber& actor, domain::AccountId to, const domain::BanknoteSet& notes
    ) {
        const auto amount = notes.total();
        if (amount.kopiykas() == 0) {
            throw DomainError(ErrorReason::InvalidAmount);
        }

        domain::Transaction tx(storage_);
        (void)detail::requireActiveClient(storage_, actor);
        auto account = detail::requireOwnedAccount(storage_, actor, to);

        auto& cash = storage_.cash();
        const auto updatedCash = cash.load().plus(notes);
        account.credit(amount);

        const auto now = clock_();
        domain::JournalEntry entry{};
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = domain::EntryKind::Deposit;
        entry.actor = actor;
        entry.account = to;
        entry.amount = amount;
        entry.cash = notes;

        storage_.accounts().save(account);
        cash.save(updatedCash);
        auto saved = storage_.journal().append(entry);

        tx.commit();
        return saved;
    }

}  // namespace atm::application

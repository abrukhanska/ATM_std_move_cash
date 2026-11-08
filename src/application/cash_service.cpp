#include "application/cash_service.hpp"

#include <type_traits>
#include <utility>

#include "application/detail/access.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/istorage.hpp"
#include "domain/transaction.hpp"

namespace atm::application {

    using domain::DomainError;
    using domain::ErrorReason;

    static_assert(std::is_nothrow_move_constructible_v<domain::JournalEntry>,
        "Returning a cash operation result must not throw after commit");

    CashService::CashService(domain::IStorage& storage, domain::Clock clock)
        : storage_(storage), clock_(std::move(clock)) {
        if (!clock_) {
            throw DomainError(ErrorReason::InvalidData);
        }
    }

    domain::BanknoteSet CashService::inventory(const domain::CardNumber& actor) const {
        (void)detail::requireOperator(storage_, actor);
        return storage_.cash().load();
    }

    domain::JournalEntry CashService::load(const domain::CardNumber& actor, const domain::BanknoteSet& notes) {
        return apply(actor, notes, Operation::Load);
    }

    domain::JournalEntry CashService::unload(const domain::CardNumber& actor, const domain::BanknoteSet& notes) {
        return apply(actor, notes, Operation::Unload);
    }

    domain::JournalEntry CashService::apply(const domain::CardNumber& actor, const domain::BanknoteSet& notes,
        Operation operation) {
        const domain::Money total = notes.total();
        if (total.kopiykas() == 0) {
            throw DomainError(ErrorReason::InvalidAmount);
        }

        domain::Transaction tx(storage_);
        (void)detail::requireOperator(storage_, actor);
        auto& cash = storage_.cash();
        const auto current = cash.load();
        const bool isLoad = operation == Operation::Load;
        const auto updated = isLoad ? current.plus(notes) : current.minus(notes);

        const auto now = clock_();
        domain::JournalEntry entry{};
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = isLoad ? domain::EntryKind::CashLoaded : domain::EntryKind::CashUnloaded;
        entry.actor = actor;
        entry.amount = total;
        entry.cash = notes;

        cash.save(updated);
        auto saved = storage_.journal().append(entry);

        tx.commit();
        return saved;
    }

}  // namespace atm::application

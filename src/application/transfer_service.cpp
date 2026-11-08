#include "application/transfer_service.hpp"

#include <type_traits>
#include <utility>

#include "application/detail/access.hpp"
#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/istorage.hpp"
#include "domain/transaction.hpp"

namespace atm::application {

    using domain::DomainError;
    using domain::ErrorReason;

    static_assert(std::is_nothrow_move_constructible_v<domain::JournalEntry>,
        "Returning a transfer result must not throw after commit");

    TransferService::TransferService(domain::IStorage& storage, domain::Clock clock)
        : storage_(storage), clock_(std::move(clock)) {
        if (!clock_) {
            throw DomainError(ErrorReason::InvalidData);
        }
    }

    TransferPreview TransferService::preview(
        const domain::CardNumber& actor, domain::AccountId from,
        const domain::CardNumber& toCard, domain::Money amount
    ) const {
        if (amount.kopiykas() == 0) {
            throw DomainError(ErrorReason::InvalidAmount);
        }
        (void)detail::requireActiveClient(storage_, actor);
        (void)detail::requireOwnedAccount(storage_, actor, from);

        if (toCard == actor) {
            throw DomainError(ErrorReason::SelfTransfer);
        }

        // Retained client cards can receive transfers.
        const auto recipient = storage_.cards().find(toCard);
        if (!recipient || recipient->kind() != domain::CardKind::Client) {
            throw DomainError(ErrorReason::UnknownCard);
        }
        const auto primary = storage_.accounts().findPrimary(toCard);
        if (!primary) {
            throw DomainError(ErrorReason::UnknownAccount);
        }

        return TransferPreview{ toCard, primary->id(), recipient->holderName(), amount };
    }

    domain::JournalEntry TransferService::execute(
        const domain::CardNumber& actor, domain::AccountId from, const TransferPreview& preview
    ) {
        if (preview.amount.kopiykas() == 0) {
            throw DomainError(ErrorReason::InvalidAmount);
        }

        domain::Transaction tx(storage_);
        (void)detail::requireActiveClient(storage_, actor);
        auto sender = detail::requireOwnedAccount(storage_, actor, from);
        if (preview.toCard == actor) {
            throw DomainError(ErrorReason::SelfTransfer);
        }

        const auto recipientCard = storage_.cards().find(preview.toCard);
        if (!recipientCard || recipientCard->kind() != domain::CardKind::Client) {
            throw DomainError(ErrorReason::InvalidPreview);
        }
        auto& accounts = storage_.accounts();
        auto recipient = accounts.findPrimary(preview.toCard);
        if (!recipient || recipient->id() != preview.toAccount ||
            recipientCard->holderName() != preview.recipientName) {
            throw DomainError(ErrorReason::InvalidPreview);
        }

        sender.debit(preview.amount);
        recipient->credit(preview.amount);

        const auto now = clock_();
        domain::JournalEntry entry{};
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = domain::EntryKind::Transfer;
        entry.actor = actor;
        entry.account = from;
        entry.target = recipient->id();
        entry.amount = preview.amount;

        accounts.save(sender);
        accounts.save(*recipient);
        auto saved = storage_.journal().append(entry);

        tx.commit();
        return saved;
    }

}  // namespace atm::application

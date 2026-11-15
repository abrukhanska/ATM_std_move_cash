#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"

#include "application/transfer_service.hpp"

#include "domain/card_number.hpp"
#include "domain/domain_error.hpp"
#include "domain/money.hpp"

#include <stdexcept>
#include <string>

namespace atm::presentation {

    Nav transferDialog(AtmContext& ctx, Session& session)
    {
        if (!session.account.has_value()) {
            ctx.ui.showError("No account selected.");
            return Nav::Cancel;
        }

        const Reply cardInput = ctx.ui.prompt(
            "Enter recipient card number (16 digits):"
        );

        if (cardInput.nav != Nav::Ok) {
            return cardInput.nav;
        }

        domain::CardNumber recipientCard;

        try {
            recipientCard = domain::CardNumber::parse(cardInput.text);
        }
        catch (const std::invalid_argument&) {
            ctx.ui.showError("Invalid card number.");
            return Nav::Cancel;
        }

        const Reply amountInput = ctx.ui.prompt(
            "Enter transfer amount in UAH:"
        );

        if (amountInput.nav != Nav::Ok) {
            return amountInput.nav;
        }

        domain::Money amount;

        try {
            std::size_t pos = 0;
            const long long value = std::stoll(amountInput.text, &pos);

            if (pos != amountInput.text.size() || value <= 0) {
                ctx.ui.showError("Invalid transfer amount.");
                return Nav::Cancel;
            }

            amount = domain::Money::uah(value);
        }
        catch (const std::invalid_argument&) {
            ctx.ui.showError("Invalid transfer amount.");
            return Nav::Cancel;
        }
        catch (const std::out_of_range&) {
            ctx.ui.showError("Transfer amount is too large.");
            return Nav::Cancel;
        }
        catch (const domain::DomainError& e) {
            if (e.reason() == domain::ErrorReason::InvalidAmount ||
                e.reason() == domain::ErrorReason::Overflow) {
                ctx.ui.showError("Invalid transfer amount.");
                return Nav::Cancel;
            }

            throw;
        }

        application::TransferPreview preview;

        try {
            preview = ctx.transfer.preview(
                session.card,
                *session.account,
                recipientCard,
                amount
            );
        }
        catch (const domain::DomainError& e) {
            switch (e.reason()) {
            case domain::ErrorReason::UnknownCard:
                ctx.ui.showError("Recipient card not found.");
                return Nav::Cancel;

            case domain::ErrorReason::SelfTransfer:
                ctx.ui.showError("Cannot transfer to your own card.");
                return Nav::Cancel;

            case domain::ErrorReason::InsufficientFunds:
                ctx.ui.showError("Insufficient funds.");
                return Nav::Cancel;

            case domain::ErrorReason::InvalidAmount:
                ctx.ui.showError("Invalid transfer amount.");
                return Nav::Cancel;

            default:
                throw;
            }
        }

        ctx.ui.showScreen("Transfer details", {
            "Recipient: " + preview.recipientName,
            "Card: " + preview.toCard.masked(),
            "Amount: " + preview.amount.format()
            });

        const Nav confirmation = ctx.ui.confirm(
            "Confirm transfer?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        domain::JournalEntry entry;

        try {
            entry = ctx.transfer.execute(
                session.card,
                *session.account,
                preview
            );
        }
        catch (const domain::DomainError& e) {
            switch (e.reason()) {
            case domain::ErrorReason::InsufficientFunds:
                ctx.ui.showError("Insufficient funds.");
                return Nav::Cancel;

            case domain::ErrorReason::InvalidPreview:
                ctx.ui.showError("Transfer details are no longer valid.");
                return Nav::Cancel;

            default:
                throw;
            }
        }

        ctx.ui.showScreen("Transfer successful", {
            "Recipient: " + preview.recipientName,
            "Amount: " + preview.amount.format()
            });

        const Nav receiptChoice = ctx.ui.confirm("Print receipt?");

        if (receiptChoice == Nav::Ok) {
            ctx.printer.print(entry);
        }

        return receiptChoice == Nav::Exit
            ? Nav::Exit
            : Nav::Ok;
    }

} // namespace atm::presentation

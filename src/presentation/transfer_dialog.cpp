#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/input_parsing.hpp"

#include "application/transfer_service.hpp"

#include "domain/card_number.hpp"
#include "domain/domain_error.hpp"
#include "domain/money.hpp"

#include <stdexcept>
#include <string>
#include <limits>
#include <ios>

namespace atm::presentation {

    Nav transferDialog(AtmContext& ctx, Session& session)
    {
        if (!session.account.has_value()) {
            ctx.ui.showError("No account selected.");
            return Nav::Cancel;
        }

        domain::CardNumber recipientCard;

        while (true) {
            const Reply cardInput = ctx.ui.prompt(
                "Enter recipient card number (16 digits):"
            );

            if (cardInput.nav != Nav::Ok) {
                return cardInput.nav;
            }

            try {
                recipientCard = parseCardNumber(cardInput.text);
                break;
            }
            catch (const std::invalid_argument&) {
                ctx.ui.showError("Invalid card number.");
            }
        }

        domain::Money amount;

        while (true) {
            const Reply amountInput = ctx.ui.prompt(
                "Enter transfer amount in UAH:"
            );

            if (amountInput.nav != Nav::Ok) {
                return amountInput.nav;
            }

            try {
                amount = parseMoney(amountInput.text);
                break;
            }
            catch (const std::invalid_argument&) {
                ctx.ui.showError("Invalid transfer amount.");
            }
            catch (const std::out_of_range&) {
                ctx.ui.showError("Transfer amount is too large.");
            }
            catch (const domain::DomainError& e) {
                if (e.reason() == domain::ErrorReason::InvalidAmount ||
                    e.reason() == domain::ErrorReason::Overflow) {
                    ctx.ui.showError("Invalid transfer amount.");
                    continue;
                }

                throw;
            }
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

            case domain::ErrorReason::UnknownAccount:
                ctx.ui.showError("Account not found.");
                return Nav::Cancel;

            case domain::ErrorReason::AccessDenied:
                ctx.ui.showError("Access denied.");
                return Nav::Cancel;

            default:
                throw;
            }
        }

        ctx.ui.showScreen("Transfer details", {
            "Recipient: " + preview.recipientName,
            "Card: " + preview.toCard.masked(),
            "Amount: " + preview.amount.format() + " UAH"
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

            case domain::ErrorReason::Overflow:
                ctx.ui.showError("Transfer failed: the amount is too large to process.");
                return Nav::Cancel;

            case domain::ErrorReason::UnknownAccount:
                ctx.ui.showError("Account not found.");
                return Nav::Cancel;

            case domain::ErrorReason::AccessDenied:
                ctx.ui.showError("Access denied.");
                return Nav::Cancel;

            default:
                throw;
            }
        }

        ctx.ui.showScreen("Transfer successful", {
            "Recipient: " + preview.recipientName,
            "Amount: " + preview.amount.format() + " UAH"
            });

        const Nav receiptChoice = ctx.ui.confirm("Print receipt?");

        if (receiptChoice == Nav::Ok) {
            try {
                ctx.printer.print(entry);
            }
            catch (const std::ios_base::failure&) {
                try {
                    ctx.ui.showError(
                        "Transfer completed, but receipt printing failed."
                    );
                }
                catch (const std::ios_base::failure&) {
                }

                return Nav::Exit;
            }
        }

        return receiptChoice == Nav::Exit
            ? Nav::Exit
            : Nav::Ok;
    }

} // namespace atm::presentation

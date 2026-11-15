#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"

#include "application/payment_service.hpp"

#include "domain/payee.hpp"
#include "domain/enums.hpp"
#include "domain/domain_error.hpp"
#include "domain/money.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace atm::presentation {

    Nav paymentDialog(AtmContext& ctx, Session& session)
    {
        if (!session.account.has_value()) {
            ctx.ui.showError("No account selected.");
            return Nav::Cancel;
        }

        const Reply categoryChoice = ctx.ui.choose(
            "Select payment category",
            { "Mobile operator", "Charity" }
        );

        if (categoryChoice.nav != Nav::Ok) {
            return categoryChoice.nav;
        }

        if (!categoryChoice.index.has_value() ||
            *categoryChoice.index >= 2) {
            ctx.ui.showError("Invalid category.");
            return Nav::Cancel;
        }

        const domain::PayeeCategory category =
            *categoryChoice.index == 0
            ? domain::PayeeCategory::MobileOperator
            : domain::PayeeCategory::Charity;

        const auto payees = ctx.payment.payees(category);

        if (payees.empty()) {
            ctx.ui.showError("No payees available.");
            return Nav::Cancel;
        }

        std::vector<std::string> options;

        for (const auto& payee : payees) {
            options.push_back(payee.name);
        }

        const Reply payeeChoice = ctx.ui.choose(
            "Select payee",
            options
        );

        if (payeeChoice.nav != Nav::Ok) {
            return payeeChoice.nav;
        }

        if (!payeeChoice.index.has_value() ||
            *payeeChoice.index >= payees.size()) {
            ctx.ui.showError("Invalid payee.");
            return Nav::Cancel;
        }

        const domain::Payee& selectedPayee =
            payees[*payeeChoice.index];

        const Reply requisiteInput = ctx.ui.prompt(
            "Enter payment requisite:"
        );

        if (requisiteInput.nav != Nav::Ok) {
            return requisiteInput.nav;
        }

        const Reply amountInput = ctx.ui.prompt(
            "Enter payment amount in UAH:"
        );

        if (amountInput.nav != Nav::Ok) {
            return amountInput.nav;
        }

        domain::Money amount;

        try {
            std::size_t pos = 0;
            const long long value = std::stoll(amountInput.text, &pos);

            if (pos != amountInput.text.size() || value <= 0) {
                ctx.ui.showError("Invalid payment amount.");
                return Nav::Cancel;
            }

            amount = domain::Money::uah(value);
        }
        catch (const std::invalid_argument&) {
            ctx.ui.showError("Invalid payment amount.");
            return Nav::Cancel;
        }
        catch (const std::out_of_range&) {
            ctx.ui.showError("Payment amount is too large.");
            return Nav::Cancel;
        }
        catch (const domain::DomainError& e) {
            if (e.reason() == domain::ErrorReason::InvalidAmount ||
                e.reason() == domain::ErrorReason::Overflow) {
                ctx.ui.showError("Invalid payment amount.");
                return Nav::Cancel;
            }

            throw;
        }

        ctx.ui.showScreen("Payment details", {
            "Payee: " + selectedPayee.name,
            "Requisite: " + requisiteInput.text,
            "Amount: " + amount.format()
            });

        const Nav confirmation = ctx.ui.confirm(
            "Confirm payment?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        domain::JournalEntry entry;


        try {
            entry = ctx.payment.pay(
                session.card,
                *session.account,
                selectedPayee.id,
                requisiteInput.text,
                amount
            );
        }
        catch (const domain::DomainError& e) {
            switch (e.reason()) {
            case domain::ErrorReason::InsufficientFunds:
                ctx.ui.showError("Insufficient funds.");
                return Nav::Cancel;

            case domain::ErrorReason::InvalidRequisite:
                ctx.ui.showError("Invalid payment requisite.");
                return Nav::Cancel;

            case domain::ErrorReason::UnknownPayee:
                ctx.ui.showError("Payee not found.");
                return Nav::Cancel;

            case domain::ErrorReason::InvalidAmount:
                ctx.ui.showError("Invalid payment amount.");
                return Nav::Cancel;

            default:
                throw;
            }
        }

        ctx.ui.showScreen("Payment successful", {
            "Payee: " + selectedPayee.name,
            "Amount: " + amount.format()
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

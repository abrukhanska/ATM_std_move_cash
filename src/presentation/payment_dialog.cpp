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

#include <cstdint>
#include <limits>
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

        // Charity does not require a payment requisite.
        std::string requisite;

        if (category == domain::PayeeCategory::MobileOperator) {
            const Reply requisiteInput = ctx.ui.prompt(
                "Enter phone number (10 digits):"
            );

            if (requisiteInput.nav != Nav::Ok) {
                return requisiteInput.nav;
            }

            requisite = requisiteInput.text;
        }

        const Reply amountInput = ctx.ui.prompt(
            "Enter payment amount in UAH:"
        );

        if (amountInput.nav != Nav::Ok) {
            return amountInput.nav;
        }

        domain::Money amount;

        try {
            const std::string& text = amountInput.text;
            const std::size_t dot = text.find_first_of(".,");

            if (text.empty() ||
                (dot != std::string::npos &&
                    (dot == 0 ||
                        dot == text.size() - 1 ||
                        text.find_first_of(".,", dot + 1) != std::string::npos ||
                        text.size() - dot - 1 > 2))) {
                throw std::invalid_argument("Invalid amount");
            }

            const std::size_t wholeEnd =
                dot == std::string::npos ? text.size() : dot;

            std::int64_t kopiykas = 0;
            const std::int64_t maxValue =
                (std::numeric_limits<std::int64_t>::max)();

            // Parse whole hryvnias without floating-point arithmetic.
            for (std::size_t i = 0; i < wholeEnd; ++i) {
                if (text[i] < '0' || text[i] > '9') {
                    throw std::invalid_argument("Invalid amount");
                }

                const int digit = text[i] - '0';

                if (kopiykas > (maxValue - digit) / 10) {
                    throw std::out_of_range("Amount too large");
                }

                kopiykas = kopiykas * 10 + digit;
            }

            if (kopiykas > maxValue / 100) {
                throw std::out_of_range("Amount too large");
            }

            kopiykas *= 100;

            // Parse optional fractional part (one or two digits).
            if (dot != std::string::npos) {
                const int first = text[dot + 1] - '0';

                if (first < 0 || first > 9) {
                    throw std::invalid_argument("Invalid amount");
                }

                int fraction = first * 10;

                if (dot + 2 < text.size()) {
                    const int second = text[dot + 2] - '0';

                    if (second < 0 || second > 9) {
                        throw std::invalid_argument("Invalid amount");
                    }

                    fraction += second;
                }

                if (kopiykas > maxValue - fraction) {
                    throw std::out_of_range("Amount too large");
                }

                kopiykas += fraction;
            }

            if (kopiykas == 0) {
                throw std::invalid_argument("Invalid amount");
            }

            amount = domain::Money::ofKopiykas(kopiykas);
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
            "Requisite: " + (requisite.empty() ? "Not required" : requisite),
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
                requisite,
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

#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/input_parsing.hpp"

#include "application/withdrawal_service.hpp"
#include "domain/domain_error.hpp"
#include "domain/money.hpp"
#include "domain/enums.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <limits>
#include <ios>

namespace atm::presentation {

    Nav withdrawDialog(AtmContext& ctx, Session& session)
    {
        if (!session.account.has_value()) {
            ctx.ui.showError("No account selected.");
            return Nav::Cancel;
        }

        const auto quickAmounts = ctx.withdrawal.quickAmounts();

        std::vector<std::string> options;

        for (const auto& amount : quickAmounts) {
            options.push_back(amount.format());
        }

        options.push_back("Enter custom amount");

        const Reply choice = ctx.ui.choose("Withdraw cash", options);

        if (choice.nav != Nav::Ok) {
            return choice.nav;
        }

        if (!choice.index.has_value() ||
            *choice.index >= options.size()) {
            ctx.ui.showError("Invalid selection.");
            return Nav::Cancel;
        }

        domain::Money amount;

        if (*choice.index < quickAmounts.size()) {
            amount = quickAmounts[*choice.index];
        }

        else {
            while (true) {
                const Reply input = ctx.ui.prompt(
                    "Enter amount in UAH (multiple of 100):"
                );

                if (input.nav != Nav::Ok) {
                    return input.nav;
                }

                try {
                    amount = parseMoney(input.text);

                    if (amount.kopiykas() % 10'000 != 0) {
                        ctx.ui.showError(
                            "Amount must be positive and a multiple of 100 UAH."
                        );
                        continue;
                    }

                    break;
                }
                catch (const std::invalid_argument&) {
                    ctx.ui.showError("Invalid amount.");
                }
                catch (const std::out_of_range&) {
                    ctx.ui.showError("Amount is too large.");
                }
                catch (const domain::DomainError& e) {
                    if (e.reason() == domain::ErrorReason::InvalidAmount ||
                        e.reason() == domain::ErrorReason::Overflow) {
                        ctx.ui.showError("Invalid amount.");
                        continue;
                    }

                    throw;
                }
            }
        }


        const Nav confirmation = ctx.ui.confirm(
            "Withdraw " + amount.format() + "?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        std::optional<domain::JournalEntry> entry;

        try {
            entry = ctx.withdrawal.withdraw(
                session.card,
                *session.account,
                amount
            );
        }
        catch (const domain::DomainError& e) {
            switch (e.reason()) {
            case domain::ErrorReason::InsufficientFunds:
                ctx.ui.showError("Insufficient funds.");
                return Nav::Cancel;

            case domain::ErrorReason::DailyLimitExceeded:
                ctx.ui.showError("Daily withdrawal limit exceeded.");
                return Nav::Cancel;

            case domain::ErrorReason::AtmOutOfCash:
                ctx.ui.showError("ATM has insufficient cash.");
                return Nav::Cancel;

            case domain::ErrorReason::InvalidAmount:
                ctx.ui.showError("Invalid withdrawal amount.");
                return Nav::Cancel;

            default:
                throw;
            }
        }


        std::vector<std::string> details{
            "Amount: " + amount.format() + " UAH",
            "Dispensed banknotes:"
        };

        if (entry.has_value()) {
            for (const auto denomination : domain::kAllDenominations) {
                const auto count = entry->cash.count(denomination);

                if (count > 0) {
                    details.push_back(
                        std::to_string(domain::denominationUah(denomination))
                        + " UAH x " + std::to_string(count)
                    );
                }
            }
        }

        ctx.ui.showScreen("Withdrawal successful", details);


        const Nav receiptChoice = ctx.ui.confirm("Print receipt?");


        if (receiptChoice == Nav::Ok) {
            try {
                ctx.printer.print(*entry);
            }
            catch (const std::ios_base::failure&) {
                try {
                    ctx.ui.showError(
                        "Withdrawal completed, but receipt printing failed."
                    );
                }
                catch (const std::ios_base::failure&) {
                    // The output stream may also be unavailable.
                }

                return Nav::Exit;
            }
        }


        return receiptChoice == Nav::Exit
            ? Nav::Exit
            : Nav::Ok;
    }

} // namespace atm::presentation

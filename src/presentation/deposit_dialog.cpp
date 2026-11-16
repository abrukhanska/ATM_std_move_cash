#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"

#include "application/deposit_service.hpp"

#include "domain/banknote_set.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace atm::presentation {


    Nav depositDialog(AtmContext& ctx, Session& session)
    {
        if (!session.account.has_value()) {
            ctx.ui.showError("No account selected.");
            return Nav::Cancel;
        }

        const std::vector<std::string> denominations = {
            "100 UAH",
            "200 UAH",
            "500 UAH",
            "1000 UAH",
            "2000 UAH"
        };

        domain::BanknoteSet notes;

        for (std::size_t i = 0; i < denominations.size(); ++i) {
            while (true) {
                const Reply input = ctx.ui.prompt(
                    "Number of " + denominations[i] +
                    " banknotes (0 to skip):"
                );

                if (input.nav != Nav::Ok) {
                    return input.nav;
                }

                if (input.text.empty() ||
                    input.text.find_first_not_of("0123456789") != std::string::npos) {
                    ctx.ui.showError("Invalid banknote count.");
                    continue;
                }

                try {
                    std::size_t pos = 0;
                    const auto count = std::stoull(input.text, &pos);

                    if (pos != input.text.size() ||
                        count > std::numeric_limits<std::uint32_t>::max()) {
                        ctx.ui.showError("Invalid banknote count.");
                        continue;
                    }

                    notes = notes.with(
                        domain::kAllDenominations[i],
                        static_cast<std::uint32_t>(count)
                    );

                    break;
                }
                catch (const std::invalid_argument&) {
                    ctx.ui.showError("Invalid banknote count.");
                }
                catch (const std::out_of_range&) {
                    ctx.ui.showError("Banknote count is too large.");
                }
            }
        }


        domain::Money total;

        try {
            total = notes.total();
        }
        catch (const domain::DomainError& e) {
            if (e.reason() == domain::ErrorReason::Overflow) {
                ctx.ui.showError("Deposit amount is too large.");
                return Nav::Cancel;
            }

            throw;
        }

        if (total == domain::Money::uah(0)) {
            ctx.ui.showError("Deposit amount cannot be zero.");
            return Nav::Cancel;
        }


        std::vector<std::string> lines;

        for (std::size_t i = 0; i < denominations.size(); ++i) {
            lines.push_back(
                denominations[i] + ": " +
                std::to_string(
                    notes.count(domain::kAllDenominations[i])
                )
            );
        }

        lines.push_back("Total: " + total.format());

        ctx.ui.showScreen("Deposit summary", lines);

        const Nav confirmation = ctx.ui.confirm(
            "Confirm deposit?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        domain::JournalEntry entry;

        try {
            entry = ctx.deposit.deposit(
                session.card,
                *session.account,
                notes
            );
        }

        catch (const domain::DomainError& e) {
            switch (e.reason()) {
            case domain::ErrorReason::InvalidAmount:
                ctx.ui.showError("Invalid deposit amount.");
                return Nav::Cancel;

            case domain::ErrorReason::Overflow:
                ctx.ui.showError("Deposit failed: the amount is too large to process.");
                return Nav::Cancel;

            default:
                throw;
            }
        }


        ctx.ui.showScreen("Deposit successful", {
            "Amount: " + total.format()
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

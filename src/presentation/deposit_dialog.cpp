#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/input_parsing.hpp"

#include "application/deposit_service.hpp"

#include "domain/banknote_set.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <ios>

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

        const Nav inputResult = readBanknotes(ctx, notes);

        if (inputResult != Nav::Ok) {
            return inputResult;
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
            try {
                ctx.printer.print(entry);
            }
            catch (const std::ios_base::failure&) {
                try {
                    ctx.ui.showError(
                        "Deposit completed, but receipt printing failed."
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

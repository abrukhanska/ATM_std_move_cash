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

        const Reply choice = ctx.ui.choose(
            "Select banknote denomination",
            denominations
        );

        if (choice.nav != Nav::Ok) {
            return choice.nav;
        }

        if (!choice.index.has_value() ||
            *choice.index >= denominations.size()) {
            ctx.ui.showError("Invalid denomination.");
            return Nav::Cancel;
        }

        const Reply input = ctx.ui.prompt(
            "Enter number of banknotes:"
        );

        if (input.nav != Nav::Ok) {
            return input.nav;
        }

        std::uint32_t count = 0;

        try {
            std::size_t pos = 0;
            const unsigned long long value = std::stoull(input.text, &pos);

            if (pos != input.text.size() ||
                value == 0 ||
                value > std::numeric_limits<std::uint32_t>::max() ||
                input.text.find('-') != std::string::npos) {
                ctx.ui.showError("Invalid banknote count.");
                return Nav::Cancel;
            }

            count = static_cast<std::uint32_t>(value);
        }
        catch (const std::invalid_argument&) {
            ctx.ui.showError("Invalid banknote count.");
            return Nav::Cancel;
        }
        catch (const std::out_of_range&) {
            ctx.ui.showError("Banknote count is too large.");
            return Nav::Cancel;
        }

        const domain::Denomination denomination =
            domain::kAllDenominations[*choice.index];

        domain::BanknoteSet notes;

        notes = notes.with(denomination, count);

        const domain::Money total = notes.total();

        const Nav confirmation = ctx.ui.confirm(
            "Deposit " + total.format() + "?"
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
            case domain::ErrorReason::InvalidData:
                ctx.ui.showError("Invalid deposit.");
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

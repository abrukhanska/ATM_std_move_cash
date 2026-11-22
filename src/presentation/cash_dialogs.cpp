#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/input_parsing.hpp"

#include "application/cash_service.hpp"
#include "domain/banknote_set.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace atm::presentation {

    namespace {

        const std::vector<std::string> denominations = {
            "100 UAH", "200 UAH", "500 UAH",
            "1000 UAH", "2000 UAH"
        };

        std::vector<std::string> describeNotes(
            const domain::BanknoteSet& notes)
        {
            std::vector<std::string> lines;

            for (std::size_t i = 0; i < denominations.size(); ++i) {
                lines.push_back(
                    denominations[i] + ": " +
                    std::to_string(
                        notes.count(domain::kAllDenominations[i])
                    )
                );
            }

            lines.push_back("Total: " + notes.total().format());
            return lines;
        }



        Nav changeCash(AtmContext& ctx, Session& session, bool loading)
        {
            domain::BanknoteSet notes;

            const Nav inputResult = readBanknotes(ctx, notes);
            if (inputResult != Nav::Ok) {
                return inputResult;
            }

            ctx.ui.showScreen(
                loading ? "Load cash" : "Unload cash",
                describeNotes(notes)
            );

            const Nav confirmation = ctx.ui.confirm(
                loading ? "Confirm cash loading?"
                : "Confirm cash unloading?"
            );

            if (confirmation != Nav::Ok) {
                return confirmation;
            }

            try {
                const domain::JournalEntry entry = loading
                    ? ctx.cash.load(session.card, notes)
                    : ctx.cash.unload(session.card, notes);

                ctx.ui.showScreen("Success", {
                    loading ? "Cash loaded successfully."
                            : "Cash unloaded successfully."
                    });

                return Nav::Ok;
            }
            catch (const domain::DomainError& e) {
                if (e.reason() == domain::ErrorReason::InvalidAmount ||
                    e.reason() == domain::ErrorReason::Overflow) {
                    ctx.ui.showError("Invalid banknote amount.");
                    return Nav::Cancel;
                }

                throw;
            }
        }

    } // namespace

    Nav cashInventoryDialog(AtmContext& ctx, Session& session)
    {
        const auto notes = ctx.cash.inventory(session.card);

        ctx.ui.showScreen(
            "Cash inventory",
            describeNotes(notes)
        );

        return Nav::Ok;
    }

    Nav cashLoadDialog(AtmContext& ctx, Session& session)
    {
        return changeCash(ctx, session, true);
    }

    Nav cashUnloadDialog(AtmContext& ctx, Session& session)
    {
        return changeCash(ctx, session, false);
    }

} // namespace atm::presentation

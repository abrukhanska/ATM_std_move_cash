#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"

#include "application/monitoring_service.hpp"
#include "domain/journal_entry.hpp"
#include "domain/money.hpp"

#include "application/card_administration_service.hpp"
#include "domain/card.hpp"
#include "application/device_service.hpp"

#include <string>
#include <vector>

namespace atm::presentation {

    Nav journalDialog(AtmContext& ctx, Session& session)
    {
        const auto entries = ctx.monitoring.recent(session.card);

        std::vector<std::string> lines;

        if (entries.empty()) {
            lines.push_back("No journal entries.");
        }

        for (const auto& entry : entries) {
            lines.push_back(
                "ID: " + std::to_string(entry.id) +
                " | Date: " + entry.localDay +
                " | Type: " + std::to_string(
                    static_cast<int>(entry.kind)
                ) +
                " | Amount: " + entry.amount.format()
            );
        }

        ctx.ui.showScreen("Recent operations", lines);
        return Nav::Ok;
    }

    Nav statisticsDialog(AtmContext& ctx, Session& session)
    {
        const auto loaded =
            ctx.monitoring.totalLoaded(session.card);

        const auto deposited =
            ctx.monitoring.totalDeposited(session.card);

        const auto withdrawn =
            ctx.monitoring.totalWithdrawn(session.card);

        ctx.ui.showScreen("ATM statistics", {
            "Total loaded: " + loaded.format(),
            "Total deposited: " + deposited.format(),
            "Total withdrawn: " + withdrawn.format()
            });

        return Nav::Ok;
    }


    Nav retainedCardsDialog(AtmContext& ctx, Session& session)
    {
        const auto cards =
            ctx.cardAdministration.listRetained(session.card);

        if (cards.empty()) {
            ctx.ui.showScreen("Retained cards", {
                "No retained cards."
                });
            return Nav::Ok;
        }

        std::vector<std::string> lines;

        for (const auto& card : cards) {
            lines.push_back(
                card.number().masked() +
                " | " + card.holderName()
            );
        }

        ctx.ui.showScreen("Retained cards", lines);
        return Nav::Ok;
    }

    Nav releaseCardDialog(AtmContext& ctx, Session& session)
    {
        const auto cards =
            ctx.cardAdministration.listRetained(session.card);

        if (cards.empty()) {
            ctx.ui.showScreen("Retained cards", {
                "No retained cards."
                });
            return Nav::Ok;
        }

        std::vector<std::string> options;

        for (const auto& card : cards) {
            options.push_back(
                card.number().masked() +
                " | " + card.holderName()
            );
        }

        const Reply choice = ctx.ui.choose(
            "Select card to release:",
            options
        );

        if (choice.nav != Nav::Ok) {
            return choice.nav;
        }

        if (!choice.index.has_value() ||
            *choice.index >= cards.size()) {
            ctx.ui.showError("Invalid card selection.");
            return Nav::Cancel;
        }

        const Nav confirmation = ctx.ui.confirm(
            "Confirm card release?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        const auto entry = ctx.cardAdministration.release(
            session.card,
            cards[*choice.index].number()
        );

        ctx.ui.showScreen("Card released", {
            "Card released successfully."
            });

        return Nav::Ok;
    }


    Nav deviceModeDialog(AtmContext& ctx, Session& session)
    {
        const bool currentlyEnabled = ctx.device.isInService();

        ctx.ui.showScreen("ATM mode", {
            std::string("Current mode: ") +
            (currentlyEnabled ? "In service" : "Out of service")
            });

        const Nav confirmation = ctx.ui.confirm(
            currentlyEnabled
            ? "Switch ATM to out-of-service mode?"
            : "Switch ATM to in-service mode?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        ctx.device.setInService(session.card, !currentlyEnabled);

        ctx.ui.showScreen("ATM mode", {
            "ATM mode changed successfully."
            });

        return Nav::Ok;
    }

} // namespace atm::presentation

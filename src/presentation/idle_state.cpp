#include "presentation/idle_state.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/dialogs.hpp"
#include "presentation/user_interface.hpp"
#include "application/auth_service.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"

#include <memory>
#include <stdexcept>

namespace atm::presentation {

    std::unique_ptr<AtmState> IdleState::doHandle(AtmContext& ctx)
    {
        ctx.ui.showScreen("WELCOME TO ATM", {
            "Please insert your card"
        });

        const Reply reply = ctx.ui.prompt("Enter card number:");
        if (reply.nav == Nav::Exit) {
            ctx.exitRequested = true;
            return nullptr;
        }

        if (reply.nav == Nav::Cancel) {
            return nullptr;
        }

        domain::CardNumber card;

        try {
            card = domain::CardNumber::parse(reply.text);
        }
        catch (const std::invalid_argument&) {
            ctx.ui.showError("Invalid card number. Please enter 16 digits.");
            return std::make_unique<IdleState>();
        }

        const domain::CardInspection inspection =
            ctx.auth.inspect(card);

        if (inspection == domain::CardInspection::Unknown) {
            ctx.ui.showError("Unknown card.");
            return nullptr;
        }

        if (inspection == domain::CardInspection::Retained) {
            ctx.ui.showError("Card is retained.");
            return nullptr;
        }

        return loginDialog(ctx, card);
    }

} // namespace atm::presentation

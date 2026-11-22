#include "presentation/out_of_service_state.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/dialogs.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/input_parsing.hpp"

#include "application/auth_service.hpp"
#include "application/device_service.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"

#include <memory>
#include <stdexcept>
#include <string_view>

namespace atm::presentation {

    std::unique_ptr<AtmState> OutOfServiceState::doHandle(AtmContext& ctx)
    {
        if (ctx.device.isBroken()) {
            ctx.exitRequested = true;
            return nullptr;
        }

        ctx.ui.showScreen(
            "Out of service",
            { "ATM unavailable for clients.", "Service access only." }
        );

        const Reply reply = ctx.ui.prompt("Enter service card number:");

        if (reply.nav == Nav::Exit) {
            ctx.exitRequested = true;
            return nullptr;
        }

        if (reply.nav == Nav::Cancel) {
            return nullptr;
        }

        domain::CardNumber card;

        try {
            card = parseCardNumber(reply.text);
        }

        catch (const std::invalid_argument&) {
            ctx.ui.showError("Invalid card number.");
            return nullptr;
        }

        const application::CardInspection inspection =
            ctx.auth.inspect(card);

        if (inspection != application::CardInspection::Service) {
            ctx.ui.showError("Service card required.");
            return nullptr;
        }

        return loginDialog(ctx, card);
    }

} // namespace atm::presentation


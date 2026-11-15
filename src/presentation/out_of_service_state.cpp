#include "presentation/out_of_service_state.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/dialogs.hpp"
#include "presentation/user_interface.hpp"
#include "application/auth_service.hpp"
#include "application/device_service.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"

#include <memory>

namespace atm::presentation {

    std::unique_ptr<AtmState> OutOfServiceState::doHandle(AtmContext& ctx)
    {
        if (ctx.device.isBroken()) {
            ctx.exitRequested = true;
            return nullptr;
        }

        ctx.ui.showScreen(
            "Out of service",
            { "ATM is unavailable for clients. Service access only." }
        );

        const Reply reply = ctx.ui.prompt("Enter service card number:");

        if (reply.nav == Nav::Exit) {
            ctx.exitRequested = true;
            return nullptr;
        }

        if (reply.nav == Nav::Cancel) {
            return nullptr;
        }

        const domain::CardNumber card =
            domain::CardNumber::parse(reply.text);

        const domain::CardInspection inspection =
            ctx.auth.inspect(card);

        if (inspection != domain::CardInspection::Service) {
            ctx.ui.showError("Service card required.");
            return nullptr;
        }

        return loginDialog(ctx, card);
    }

} // namespace atm::presentation

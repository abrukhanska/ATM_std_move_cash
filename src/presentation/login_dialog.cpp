#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/atm_state.hpp"
#include "presentation/client_state.hpp"
#include "presentation/service_state.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/session.hpp"

#include "application/auth_service.hpp"
#include "domain/pin_code.hpp"
#include "domain/enums.hpp"
#include "domain/domain_error.hpp"

#include <memory>
#include <utility>
#include <stdexcept>

namespace atm::presentation {


    std::unique_ptr<AtmState> loginDialog(
        AtmContext& ctx,
        const domain::CardNumber& card
    )
    {
        const auto inspection = ctx.auth.inspect(card);

        if (inspection == domain::CardInspection::Unknown ||
            inspection == domain::CardInspection::Retained) {
            ctx.ui.showError("Card is unavailable.");
            return nullptr;
        }

        const bool isService =
            inspection == domain::CardInspection::Service;

        int failedAttempts = 0;
        constexpr int maxAttempts = 3;

        while (failedAttempts < maxAttempts) {
            Reply reply = ctx.ui.promptSecret("Enter PIN:");

            if (reply.nav == Nav::Exit) {
                ctx.exitRequested = true;
                return nullptr;
            }

            if (reply.nav == Nav::Cancel) {
                return nullptr;
            }

            domain::PinCode pin;

            try {
                pin = domain::PinCode::parse(reply.text);
            }
            catch (const std::invalid_argument&) {
                ctx.ui.showError("Invalid PIN format.");
                continue;
            }

            const auto status = ctx.auth.authenticate(card, pin);

            if (status == domain::AuthStatus::Retained) {
                ctx.ui.showError("Card retained.");
                return nullptr;
            }

            if (status == domain::AuthStatus::WrongPin) {
                ++failedAttempts;
                ctx.ui.showError("Incorrect PIN.");
                continue;
            }

            if (status == domain::AuthStatus::Ok) {
                if (isService) {
                    return std::make_unique<ServiceState>(
                        Session(card, domain::CardKind::Service)
                    );
                }

                return std::make_unique<ClientState>(
                    Session(card, domain::CardKind::Client)
                );
            }
        }

        ctx.ui.showError("Maximum PIN attempts reached.");
        return nullptr;
    }


} // namespace atm::presentation

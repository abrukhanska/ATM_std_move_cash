#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"

#include "application/auth_service.hpp"

#include "domain/pin_code.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"

#include <stdexcept>
#include <string>

namespace atm::presentation {

    Nav changePinDialog(AtmContext& ctx, Session& session)
    {
        const Reply oldInput = ctx.ui.promptSecret(
            "Enter current PIN:"
        );

        if (oldInput.nav != Nav::Ok) {
            return oldInput.nav;
        }

        const Reply newInput = ctx.ui.promptSecret(
            "Enter new PIN:"
        );

        if (newInput.nav != Nav::Ok) {
            return newInput.nav;
        }

        const Reply repeatInput = ctx.ui.promptSecret(
            "Repeat new PIN:"
        );

        if (repeatInput.nav != Nav::Ok) {
            return repeatInput.nav;
        }

        if (newInput.text != repeatInput.text) {
            ctx.ui.showError("New PINs do not match.");
            return Nav::Cancel;
        }

        domain::PinCode oldPin;
        domain::PinCode newPin;

        try {
            oldPin = domain::PinCode::parse(oldInput.text);
            newPin = domain::PinCode::parse(newInput.text);
        }
        catch (const std::invalid_argument&) {
            ctx.ui.showError("PIN must contain exactly 4 digits.");
            return Nav::Cancel;
        }

        if (oldPin == newPin) {
            ctx.ui.showError("New PIN must differ from current PIN.");
            return Nav::Cancel;
        }

        try {
            ctx.auth.changePin(session.card, oldPin, newPin);
        }
        catch (const domain::DomainError& e) {
            switch (e.reason()) {
            case domain::ErrorReason::IncorrectPin:
                ctx.ui.showError("Current PIN is incorrect.");
                return Nav::Cancel;

            case domain::ErrorReason::SamePin:
                ctx.ui.showError("New PIN must differ from current PIN.");
                return Nav::Cancel;

            default:
                throw;
            }
        }

        ctx.ui.showScreen("PIN changed", {
            "Your PIN has been changed successfully."
            });

        return Nav::Ok;
    }

} // namespace atm::presentation

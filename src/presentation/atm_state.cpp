#include "presentation/atm_state.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/idle_state.hpp"
#include "presentation/out_of_service_state.hpp"
#include "presentation/user_interface.hpp"
#include "application/device_service.hpp"

#include <exception>
#include <memory>
#include <ios>

namespace atm::presentation {

    std::unique_ptr<AtmState> AtmState::handle(AtmContext& ctx)
    {
        try {
            return doHandle(ctx);
        }

        catch (const std::ios_base::failure&) {
            // Console output may be unavailable.
            // Do not continue the current session.
            ctx.exitRequested = true;
            return nullptr;
        }
        catch (const std::exception&) {
            try {
                ctx.ui.showError("Temporarily unavailable.");
            }
            catch (const std::ios_base::failure&) {
                ctx.exitRequested = true;
                return nullptr;
            }

            if (ctx.device.isBroken() || !ctx.device.isInService()) {
                return std::make_unique<OutOfServiceState>();
            }

            return std::make_unique<IdleState>();
        }

    }

} // namespace atm::presentation


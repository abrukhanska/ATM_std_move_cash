#include "presentation/service_state.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/idle_state.hpp"
#include "presentation/out_of_service_state.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/menu_item.hpp"
#include "application/device_service.hpp"

#include <memory>
#include <string>
#include <vector>

namespace atm::presentation {

    std::unique_ptr<AtmState> ServiceState::doHandle(AtmContext& ctx)
    {
        while (true) {
            if (ctx.device.isBroken()) {
                return std::make_unique<OutOfServiceState>();
            }

            std::vector<std::string> options;

            for (const auto& item : ctx.serviceMenu) {
                options.push_back(item.title);
            }

            const Reply reply = ctx.ui.choose("Service menu:", options);

            if (reply.nav == Nav::Exit) {
                if (ctx.device.isInService()) {
                    return std::make_unique<IdleState>();
                }

                return std::make_unique<OutOfServiceState>();
            }

            if (reply.nav == Nav::Cancel) {
                if (ctx.device.isInService()) {
                    return std::make_unique<IdleState>();
                }

                return std::make_unique<OutOfServiceState>();
            }

            if (!reply.index.has_value() ||
                *reply.index >= ctx.serviceMenu.size()) {
                ctx.ui.showError("Invalid menu selection.");
                continue;
            }

            const Nav result = ctx.serviceMenu[*reply.index].run(session_);

            if (ctx.device.isBroken()) {
                return std::make_unique<OutOfServiceState>();
            }

            if (result == Nav::Exit) {
                if (ctx.device.isInService()) {
                    return std::make_unique<IdleState>();
                }

                return std::make_unique<OutOfServiceState>();
            }
        }
    }

} // namespace atm::presentation

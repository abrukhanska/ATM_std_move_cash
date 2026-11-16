#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/support_channel.hpp"

#include "domain/types.hpp"

#include <string>
#include <vector>

namespace atm::presentation {

    Nav supportDialog(AtmContext& ctx, Session&)

    {
        const std::vector<std::string> problems = {
            "Transaction problem",
            "Card problem",
            "Other problem"
        };

        const Reply choice = ctx.ui.choose(
            "Select problem type:",
            problems
        );

        if (choice.nav != Nav::Ok) {
            return choice.nav;
        }

        if (!choice.index.has_value() ||
            *choice.index >= problems.size()) {
            ctx.ui.showError("Invalid problem type.");
            return Nav::Cancel;
        }

        const std::string message =
            "Support request\n"
            "Problem: " + problems[*choice.index] + "\n"
            "Time (Unix): " +
            std::to_string(domain::toEpochSeconds(ctx.clock()));

        const Nav confirmation = ctx.ui.confirm(
            "Send support request?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        ctx.support.send(message);

        ctx.ui.showScreen("Support", {
            "Your support request has been sent."
            });

        return Nav::Ok;
    }

} // namespace atm::presentation

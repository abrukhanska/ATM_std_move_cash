#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"

#include "application/account_service.hpp"
#include "domain/money.hpp"

namespace atm::presentation {

    Nav balanceDialog(AtmContext& ctx, Session& session)
    {
        if (!session.account.has_value()) {
            ctx.ui.showError("No account selected.");
            return Nav::Cancel;
        }

        const domain::Money balance =
            ctx.accounts.balance(session.card, *session.account);

        ctx.ui.showScreen("Account balance", {
            "Balance: " + balance.format()
            });

        return Nav::Ok;
    }

} // namespace atm::presentation

#include "presentation/client_state.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/idle_state.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/menu_item.hpp"
#include "application/account_service.hpp"

#include <memory>
#include <string>
#include <vector>

namespace atm::presentation {

    std::unique_ptr<AtmState> ClientState::doHandle(AtmContext& ctx)
    {
        if (!session_.account.has_value()) {
            const auto accounts = ctx.accounts.accountsOf(session_.card);

            if (accounts.empty()) {
                ctx.ui.showError("No accounts available.");
                return std::make_unique<IdleState>();
            }

            if (accounts.size() == 1) {
                session_.account = accounts[0].id();
            }
            else {
                std::vector<std::string> options;

                for (const auto& account : accounts) {
                    options.push_back(
                        "Account " + std::to_string(account.id())
                    );
                }

                const Reply reply = ctx.ui.choose("Select account:", options);

                if (reply.nav != Nav::Ok) {
                    return std::make_unique<IdleState>();
                }

                if (!reply.index.has_value() ||
                    *reply.index >= accounts.size()) {
                    ctx.ui.showError("Invalid account selection.");
                    return nullptr;
                }

                session_.account = accounts[*reply.index].id();
            }
        }

        while (true) {
            std::vector<std::string> options;

            for (const auto& item : ctx.clientMenu) {
                options.push_back(item.title);
            }

            const Reply reply = ctx.ui.choose("Client menu:", options);

            if (reply.nav == Nav::Exit) {
                return std::make_unique<IdleState>();
            }

            if (reply.nav == Nav::Cancel) {
                return std::make_unique<IdleState>();
            }

            if (!reply.index.has_value() ||
                *reply.index >= ctx.clientMenu.size()) {
                ctx.ui.showError("Invalid menu selection.");
                continue;
            }

            const Nav result = ctx.clientMenu[*reply.index].run(session_);

            if (result == Nav::Exit) {
                return std::make_unique<IdleState>();
            }
        }
    }

} // namespace atm::presentation

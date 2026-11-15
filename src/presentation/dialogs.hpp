#ifndef ATM_PRESENTATION_DIALOGS_HPP
#define ATM_PRESENTATION_DIALOGS_HPP

#include "presentation/ui_types.hpp"
#include "domain/card_number.hpp"

#include <memory>

namespace atm::presentation {

    class AtmContext;
    class AtmState;
    struct Session;

    //  Authorization Dialog
    [[nodiscard]] std::unique_ptr<AtmState> loginDialog(
        AtmContext& ctx,
        const domain::CardNumber& card
    );

    //  Client Operations
    Nav balanceDialog(AtmContext& ctx, Session& session);
    Nav withdrawDialog(AtmContext& ctx, Session& session);
    Nav depositDialog(AtmContext& ctx, Session& session);
    Nav transferDialog(AtmContext& ctx, Session& session);
    Nav paymentDialog(AtmContext& ctx, Session& session);
    Nav changePinDialog(AtmContext& ctx, Session& session);
    Nav supportDialog(AtmContext& ctx, Session& session);

    // Service Operations
    Nav cashInventoryDialog(AtmContext& ctx, Session& session);
    Nav cashLoadDialog(AtmContext& ctx, Session& session);
    Nav cashUnloadDialog(AtmContext& ctx, Session& session);

    Nav journalDialog(AtmContext& ctx, Session& session);
    Nav statisticsDialog(AtmContext& ctx, Session& session);

    Nav retainedCardsDialog(AtmContext& ctx, Session& session);
    Nav releaseCardDialog(AtmContext& ctx, Session& session);

    Nav deviceModeDialog(AtmContext& ctx, Session& session);

} // namespace atm::presentation

#endif // ATM_PRESENTATION_DIALOGS_HPP

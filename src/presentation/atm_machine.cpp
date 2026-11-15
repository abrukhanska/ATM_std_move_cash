
#include "presentation/atm_machine.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/support_channel.hpp"
#include "application/device_service.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace atm::presentation {

    AtmMachine::AtmMachine(
        AtmContext& ctx,
        std::unique_ptr<AtmState> initialState
    )
        : ctx_(ctx),
        state_(std::move(initialState))
    {
        if (!state_) {
            throw std::invalid_argument(
                "AtmMachine requires an initial state"
            );
        }
    }

    void AtmMachine::step()
    {
        if (ctx_.exitRequested) {
            return;
        }

        if (ctx_.device.isBroken()) {
            state_.reset();
            ctx_.exitRequested = true;

            if (!supportAttempted_) {
                supportAttempted_ = true;

                try {
                    ctx_.support.send(
                        "ATM storage failure. Service required."
                    );
                }
                catch (const std::exception&) {
                }
            }

            return;
        }

        std::unique_ptr<AtmState> nextState =
            state_->handle(ctx_);

        if (nextState) {
            state_ = std::move(nextState);
        }

        if (ctx_.device.isBroken()) {
            state_.reset();
            ctx_.exitRequested = true;

            if (!supportAttempted_) {
                supportAttempted_ = true;

                try {
                    ctx_.support.send(
                        "ATM storage failure. Service required."
                    );
                }
                catch (const std::exception&) {
                }
            }
        }
    }

    void AtmMachine::run()
    {
        while (!ctx_.exitRequested) {
            step();
        }
    }

} // namespace atm::presentation

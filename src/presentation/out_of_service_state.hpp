#ifndef ATM_PRESENTATION_OUT_OF_SERVICE_STATE_HPP
#define ATM_PRESENTATION_OUT_OF_SERVICE_STATE_HPP

#include "presentation/atm_state.hpp"

namespace atm::presentation {

    class OutOfServiceState : public AtmState {
    private:
        std::unique_ptr<AtmState> doHandle(AtmContext& ctx) override;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_OUT_OF_SERVICE_STATE_HPP

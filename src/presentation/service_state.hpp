#ifndef ATM_PRESENTATION_SERVICE_STATE_HPP
#define ATM_PRESENTATION_SERVICE_STATE_HPP

#include "presentation/atm_state.hpp"
#include "presentation/session.hpp"

#include <utility>

namespace atm::presentation {

    class ServiceState : public AtmState {
    public:
        explicit ServiceState(Session session)
            : session_(std::move(session)) {
        }

    private:
        Session session_;

        std::unique_ptr<AtmState> doHandle(AtmContext& ctx) override;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_SERVICE_STATE_HPP

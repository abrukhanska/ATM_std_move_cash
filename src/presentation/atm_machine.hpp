#ifndef ATM_PRESENTATION_ATM_MACHINE_HPP
#define ATM_PRESENTATION_ATM_MACHINE_HPP

#include "presentation/atm_state.hpp"

#include <memory>

namespace atm::presentation {

    class AtmContext;

    class AtmMachine {
    public:
        explicit AtmMachine(
            AtmContext& ctx,
            std::unique_ptr<AtmState> initialState
        );

        AtmMachine(const AtmMachine&) = delete;
        AtmMachine& operator=(const AtmMachine&) = delete;
        AtmMachine(AtmMachine&&) = delete;
        AtmMachine& operator=(AtmMachine&&) = delete;

        void step();
        void run();

    private:
        AtmContext& ctx_;
        std::unique_ptr<AtmState> state_;
        bool supportAttempted_{ false };
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_ATM_MACHINE_HPP

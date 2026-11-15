#ifndef ATM_PRESENTATION_ATM_STATE_HPP
#define ATM_PRESENTATION_ATM_STATE_HPP

#include <memory>

namespace atm::presentation {

    class AtmContext;

    class AtmState {
    public:
        virtual ~AtmState() noexcept = default;

        std::unique_ptr<AtmState> handle(AtmContext& ctx);

    protected:
        virtual std::unique_ptr<AtmState> doHandle(AtmContext& ctx) = 0;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_ATM_STATE_HPP
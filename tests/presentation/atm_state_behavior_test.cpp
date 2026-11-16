#include "support/mini_test.hpp"
#include "support/test_atm_context.hpp"

#include "presentation/atm_machine.hpp"
#include "presentation/atm_context.hpp"
#include "presentation/atm_state.hpp"
#include "presentation/idle_state.hpp"
#include "presentation/client_state.hpp"
#include "presentation/service_state.hpp"
#include "presentation/out_of_service_state.hpp"
#include "presentation/session.hpp"
#include "presentation/ui_types.hpp"

#include "domain/card.hpp"
#include "domain/account.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/money.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

    using namespace atm::presentation;
    using namespace atm::domain;

    const auto clientCard = CardNumber::parse("4000000000000001");
    const auto serviceCard = CardNumber::parse("4000000000000003");

    void seedClient(atm::test::TestAtmContext& f) {
        f.storage.seed(Card(
            clientCard, "Alice", "test-hash", CardKind::Client
        ));
        f.storage.seed(Account(
            1, clientCard, Money::uah(5000), true
        ));
    }

    void seedService(atm::test::TestAtmContext& f) {
        f.storage.seed(Card(
            serviceCard, "Operator", "test-hash", CardKind::Service
        ));
    }

    class ThrowingState final : public AtmState {
    protected:
        std::unique_ptr<AtmState> doHandle(AtmContext&) override {
            throw std::runtime_error("Injected state failure");
        }
    };

    TEST(AtmMachine, RejectsNullInitialState) {
        atm::test::TestAtmContext f;

        bool rejected = false;

        try {
            AtmMachine machine(f.ctx, nullptr);
        }
        catch (const std::invalid_argument&) {
            rejected = true;
        }

        EXPECT_TRUE(rejected);
    }

    TEST(AtmMachine, DoesNotProcessAfterExitRequested) {
        atm::test::TestAtmContext f;
        f.ctx.exitRequested = true;

        AtmMachine machine(
            f.ctx,
            std::make_unique<IdleState>()
        );

        machine.step();

        EXPECT_TRUE(f.ctx.exitRequested);
        EXPECT_TRUE(f.output.str().empty());
    }

    TEST(AtmState, HandlesExceptionWithoutCrashing) {
        atm::test::TestAtmContext f;
        ThrowingState state;

        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next != nullptr);
        EXPECT_TRUE(
            dynamic_cast<IdleState*>(next.get()) != nullptr
        );
        EXPECT_TRUE(
            f.output.str().find("Temporarily unavailable.")
            != std::string::npos
        );
    }

    TEST(IdleState, EofRequestsExit) {
        atm::test::TestAtmContext f;

        AtmMachine machine(
            f.ctx,
            std::make_unique<IdleState>()
        );

        machine.step();

        EXPECT_TRUE(f.ctx.exitRequested);
    }

    TEST(IdleState, InvalidCardReturnsToIdle) {
        atm::test::TestAtmContext f;
        f.input.str("invalid-card\n");

        IdleState state;
        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next != nullptr);
        EXPECT_TRUE(
            dynamic_cast<IdleState*>(next.get()) != nullptr
        );
        EXPECT_TRUE(
            f.output.str().find("Invalid card number.")
            != std::string::npos
        );
    }

    TEST(IdleState, UnknownCardDoesNotStartSession) {
        atm::test::TestAtmContext f;
        f.input.str("4000000000000099\n");

        IdleState state;
        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next == nullptr);
        EXPECT_EQ(f.storage.commits, 0);
    }

    TEST(ClientState, CancelReturnsToIdleWithoutChanges) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("0\n");

        ClientState state(
            Session(clientCard, CardKind::Client, 1)
        );

        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next != nullptr);
        EXPECT_TRUE(
            dynamic_cast<IdleState*>(next.get()) != nullptr
        );
        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            Money::uah(5000)
        );
    }

    TEST(ClientState, MenuExitReturnsToIdle) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("exit\n");

        ClientState state(
            Session(clientCard, CardKind::Client, 1)
        );

        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next != nullptr);
        EXPECT_TRUE(
            dynamic_cast<IdleState*>(next.get()) != nullptr
        );
    }

    TEST(ServiceState, ExitReturnsToIdleWhenInService) {
        atm::test::TestAtmContext f;
        seedService(f);

        f.input.str("exit\n");

        ServiceState state(
            Session(serviceCard, CardKind::Service)
        );

        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next != nullptr);
        EXPECT_TRUE(
            dynamic_cast<IdleState*>(next.get()) != nullptr
        );
    }

    TEST(OutOfServiceState, EofRequestsExit) {
        atm::test::TestAtmContext f;

        OutOfServiceState state;
        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next == nullptr);
        EXPECT_TRUE(f.ctx.exitRequested);
    }

    TEST(OutOfServiceState, ClientCardIsRejected) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("4000000000000001\n");

        OutOfServiceState state;
        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next == nullptr);
        EXPECT_TRUE(
            f.output.str().find("Service card required.")
            != std::string::npos
        );
        EXPECT_EQ(f.storage.commits, 0);
    }

} // namespace

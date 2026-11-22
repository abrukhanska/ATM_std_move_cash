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
#include "presentation/input_parsing.hpp"

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


    TEST(ServiceState, ZeroReturnsToIdleWhenInService) {
        atm::test::TestAtmContext f;
        seedService(f);

        f.input.str("0\n");

        ServiceState state(
            Session(serviceCard, CardKind::Service)
        );

        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next != nullptr);
        EXPECT_TRUE(
            dynamic_cast<IdleState*>(next.get()) != nullptr
        );

        EXPECT_EQ(f.storage.commits, 0);
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



    TEST(OutOfServiceState, InvalidCardShowsErrorAndAllowsRetry) {
        atm::test::TestAtmContext f;
        seedService(f);

        f.input.str("invalid-card\n");

        OutOfServiceState state;
        auto next = state.handle(f.ctx);

        EXPECT_TRUE(next == nullptr);
        EXPECT_FALSE(f.ctx.exitRequested);

        const std::string output = f.output.str();

        EXPECT_NE(
            output.find("Invalid card number."),
            std::string::npos
        );

        EXPECT_EQ(f.storage.commits, 0);
    }


    TEST(InputParsing, ParseMoneyAcceptsValidAmounts) {
        using atm::presentation::parseMoney;

        EXPECT_EQ(parseMoney("100").kopiykas(), 10000);
        EXPECT_EQ(parseMoney("12.50").kopiykas(), 1250);
        EXPECT_EQ(parseMoney("12,50").kopiykas(), 1250);
        EXPECT_EQ(parseMoney("  25.5  ").kopiykas(), 2550);
        EXPECT_EQ(parseMoney("0.01").kopiykas(), 1);
    }



    TEST(InputParsing, ParseMoneyRejectsInvalidAmounts) {
        using atm::presentation::parseMoney;

        const std::string invalid[] = {
            "",
            "   ",
            "0",
            "0.00",
            "-10",
            "+10",
            "abc",
            "12abc",
            ".50",
            "12.",
            "12.345",
            "12,3.4",
            "1 000"
        };

        for (const auto& input : invalid) {
            bool rejected = false;

            try {
                (void)parseMoney(input);
            }
            catch (const std::invalid_argument&) {
                rejected = true;
            }

            EXPECT_TRUE(rejected);
        }

        bool overflowRejected = false;

        try {
            (void)parseMoney("999999999999999999999999");
        }
        catch (const std::out_of_range&) {
            overflowRejected = true;
        }

        EXPECT_TRUE(overflowRejected);
    }



    TEST(InputParsing, ParseCardNumberAcceptsValidInput) {
        using atm::presentation::parseCardNumber;

        const auto expected =
            CardNumber::parse("4000000000000001");

        EXPECT_TRUE(
            parseCardNumber("4000000000000001") == expected
        );

        EXPECT_TRUE(
            parseCardNumber("  4000000000000001  ") == expected
        );

        EXPECT_TRUE(
            parseCardNumber("\t4000000000000001\n") == expected
        );
    }


    TEST(InputParsing, ParseCardNumberRejectsInvalidInput) {
        using atm::presentation::parseCardNumber;

        const std::string invalid[] = {
            "",
            "   ",
            "\t\n",
            "1234",
            "400000000000000",
            "40000000000000012",
            "abcdefghijklmnop",
            "40000000abcd0001",
            "4000 0000 0000 0001",
            "-4000000000000001"
        };

        for (const auto& input : invalid) {
            bool rejected = false;

            try {
                (void)parseCardNumber(input);
            }
            catch (const std::invalid_argument&) {
                rejected = true;
            }

            EXPECT_TRUE(rejected);
        }
    }


    TEST(InputParsing, ReadBanknotesAcceptsValidCounts) {
        atm::test::TestAtmContext f;
        BanknoteSet notes;

        f.input.str("1\n2\n3\n4\n5\n");

        const Nav result = readBanknotes(f.ctx, notes);

        EXPECT_TRUE(result == Nav::Ok);

        for (std::size_t i = 0; i < kAllDenominations.size(); ++i) {
            EXPECT_EQ(
                notes.count(kAllDenominations[i]),
                static_cast<std::uint32_t>(i + 1)
            );
        }

        EXPECT_EQ(f.storage.commits, 0);
    }

    TEST(InputParsing, ReadBanknotesAcceptsZeroCounts) {
        atm::test::TestAtmContext f;
        BanknoteSet notes;

        f.input.str("0\n0\n0\n0\n0\n");

        const Nav result = readBanknotes(f.ctx, notes);

        EXPECT_TRUE(result == Nav::Ok);

        for (const auto denomination : kAllDenominations) {
            EXPECT_EQ(notes.count(denomination), 0);
        }

        EXPECT_EQ(f.storage.commits, 0);
    }

    TEST(InputParsing, ReadBanknotesRetriesInvalidInput) {
        atm::test::TestAtmContext f;
        BanknoteSet notes;

        // Three invalid attempts, then five valid counts.
        f.input.str(
            "abc\n"
            "-1\n"
            "1.5\n"
            "2\n"
            "0\n"
            "0\n"
            "0\n"
            "0\n"
        );

        const Nav result = readBanknotes(f.ctx, notes);

        EXPECT_TRUE(result == Nav::Ok);

        EXPECT_EQ(
            notes.count(kAllDenominations[0]),
            2
        );

        for (std::size_t i = 1; i < kAllDenominations.size(); ++i) {
            EXPECT_EQ(notes.count(kAllDenominations[i]), 0);
        }

        EXPECT_NE(
            f.output.str().find("Invalid banknote count."),
            std::string::npos
        );

        EXPECT_EQ(f.storage.commits, 0);
    }

    TEST(InputParsing, ReadBanknotesRejectsOverflowAndRetries) {
        atm::test::TestAtmContext f;
        BanknoteSet notes;

        f.input.str(
            "4294967296\n"
            "999999999999999999999999999999\n"
            "3\n"
            "0\n"
            "0\n"
            "0\n"
            "0\n"
        );

        const Nav result = readBanknotes(f.ctx, notes);

        EXPECT_TRUE(result == Nav::Ok);

        EXPECT_EQ(
            notes.count(kAllDenominations[0]),
            3
        );

        EXPECT_NE(
            f.output.str().find("Invalid banknote count."),
            std::string::npos
        );

        EXPECT_NE(
            f.output.str().find("Banknote count is too large."),
            std::string::npos
        );

        EXPECT_EQ(f.storage.commits, 0);
    }

    TEST(InputParsing, ReadBanknotesCancelPreservesEnteredCounts) {
        atm::test::TestAtmContext f;
        BanknoteSet notes;

        f.input.str("2\ncancel\n");

        const Nav result = readBanknotes(f.ctx, notes);

        EXPECT_TRUE(result == Nav::Cancel);

        EXPECT_EQ(
            notes.count(kAllDenominations[0]),
            2
        );

        for (std::size_t i = 1; i < kAllDenominations.size(); ++i) {
            EXPECT_EQ(notes.count(kAllDenominations[i]), 0);
        }

        EXPECT_EQ(f.storage.commits, 0);
    }


} // namespace

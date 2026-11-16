#include "support/mini_test.hpp"

#include "presentation/console_ui.hpp"
#include "presentation/ui_types.hpp"

#include "support/test_atm_context.hpp"
#include "presentation/dialogs.hpp"
#include "presentation/session.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"


#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/money.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace {

    using atm::presentation::ConsoleUi;
    using atm::presentation::Nav;
    using atm::presentation::Reply;

    TEST(M1Additional, SecretPromptCancel)
    {
        std::istringstream input("cancel\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.promptSecret("Enter PIN:");

        EXPECT_EQ(reply.nav, Nav::Cancel);
    }

    TEST(M1Additional, SecretPromptEof)
    {
        std::istringstream input("");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.promptSecret("Enter PIN:");

        EXPECT_EQ(reply.nav, Nav::Exit);
    }

    TEST(M1Additional, SecretPromptEmptyRetries)
    {
        std::istringstream input("\n  \n1234\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.promptSecret("Enter PIN:");

        EXPECT_EQ(reply.nav, Nav::Ok);
        EXPECT_EQ(reply.text, std::string("1234"));
    }

    TEST(M1Additional, ChooseInvalidThenCancel)
    {
        std::istringstream input("999\n0\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Client menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Cancel);
        EXPECT_FALSE(reply.index.has_value());
    }
    TEST(M1Additional, ConfirmInvalidThenCancel)
    {
        std::istringstream input("maybe\nno\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        EXPECT_EQ(ui.confirm("Confirm?"), Nav::Cancel);
    }




    TEST(M1Additional, ChooseCancelCommand)
    {
        std::istringstream input("cancel\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Client menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Cancel);
        EXPECT_FALSE(reply.index.has_value());
    }

    TEST(M1Additional, ChooseEofAfterInvalidInput)
    {
        std::istringstream input("999\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Client menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Exit);
        EXPECT_FALSE(reply.index.has_value());
    }

    TEST(M1Additional, ConfirmEofAfterInvalidInput)
    {
        std::istringstream input("maybe\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        EXPECT_EQ(ui.confirm("Confirm?"), Nav::Exit);
    }


    TEST(M1Additional, WithdrawalWithoutAccountDoesNotChangeStorage)
    {
        atm::test::TestAtmContext fixture;

        // The account has not been selected.
        // This path must return before asking for any amount.
        atm::presentation::Session session(
            atm::domain::CardNumber::parse("0000000000000001"),
            atm::domain::CardKind::Client
        );

        const auto result = atm::presentation::withdrawDialog(
            fixture.ctx,
            session
        );

        EXPECT_EQ(result, Nav::Cancel);
        EXPECT_EQ(fixture.storage.commits, 0);
        EXPECT_TRUE(fixture.storage.journal().last(1).empty());
    }



    TEST(M1Additional, WithdrawalRejectsInvalidAmountsWithoutChanges)
    {
        const std::vector<std::string> invalidAmounts{
            "abc",
            "100,123",
            "100.123",
            "100,5.0",
            "100.5,0",
            "100,50",
            "100.5",
            "999999999999999999999999999999"
        };

        for (const auto& text : invalidAmounts) {
            atm::test::TestAtmContext fixture;

            const auto card =
                atm::domain::CardNumber::parse("0000000000000001");

            atm::presentation::Session session(
                card,
                atm::domain::CardKind::Client,
                1
            );

            const std::size_t customOption =
                fixture.withdrawal.quickAmounts().size() + 1;

            fixture.input.str(
                std::to_string(customOption) + "\n" + text + "\n"
            );

            const auto result = atm::presentation::withdrawDialog(
                fixture.ctx,
                session
            );

            EXPECT_EQ(result, Nav::Cancel);
            EXPECT_EQ(fixture.storage.commits, 0);
            EXPECT_TRUE(fixture.storage.journal().last(1).empty());
        }
    }

    TEST(M1Additional, WithdrawalCancelBeforeConfirmationDoesNotChangeStorage)
    {
        atm::test::TestAtmContext fixture;

        const auto card =
            atm::domain::CardNumber::parse("0000000000000001");

        atm::presentation::Session session(
            card,
            atm::domain::CardKind::Client,
            1
        );

        const std::size_t customOption =
            fixture.withdrawal.quickAmounts().size() + 1;

        fixture.input.str(
            std::to_string(customOption) + "\n100,00\nno\n"
        );

        const auto result = atm::presentation::withdrawDialog(
            fixture.ctx,
            session
        );

        EXPECT_EQ(result, Nav::Cancel);
        EXPECT_EQ(fixture.storage.commits, 0);
        EXPECT_TRUE(fixture.storage.journal().last(1).empty());
    }

    TEST(M1Additional, WithdrawalEofBeforeConfirmationDoesNotChangeStorage)
    {
        atm::test::TestAtmContext fixture;

        const auto card =
            atm::domain::CardNumber::parse("0000000000000001");

        atm::presentation::Session session(
            card,
            atm::domain::CardKind::Client,
            1
        );

        const std::size_t customOption =
            fixture.withdrawal.quickAmounts().size() + 1;

        fixture.input.str(
            std::to_string(customOption) + "\n100.00\n"
        );

        const auto result = atm::presentation::withdrawDialog(
            fixture.ctx,
            session
        );

        EXPECT_EQ(result, Nav::Exit);
        EXPECT_EQ(fixture.storage.commits, 0);
        EXPECT_TRUE(fixture.storage.journal().last(1).empty());
    }



    // --- M1: Amount parsing and unchanged storage ---------------------

    TEST(M1Additional, WithdrawalInvalidAmountsNoChanges)
    {
        const std::vector<std::string> invalid{
            "abc",
            "100,123",
            "100.123",
            "100,5.0",
            "100.5,0",
            "100,50",
            "100.5",
            "999999999999999999999999999999"
        };

        for (const auto& value : invalid) {
            atm::test::TestAtmContext f;

            const auto card =
                atm::domain::CardNumber::parse("0000000000000001");

            f.storage.seed(atm::domain::Account(
                1, card, atm::domain::Money::uah(5000), true
            ));

            atm::presentation::Session session(
                card, atm::domain::CardKind::Client, 1
            );

            const auto before =
                f.storage.accounts().find(1)->balance();

            const auto option =
                f.withdrawal.quickAmounts().size() + 1;

            f.input.str(
                std::to_string(option) + "\n" + value + "\n"
            );

            EXPECT_EQ(
                atm::presentation::withdrawDialog(f.ctx, session),
                Nav::Cancel
            );

            EXPECT_EQ(f.storage.commits, 0);
            EXPECT_EQ(f.storage.accounts().find(1)->balance(), before);
            EXPECT_TRUE(f.storage.journal().last(1).empty());
        }
    }

    TEST(M1Additional, WithdrawalValidAmountsCancelledNoChanges)
    {
        const std::vector<std::string> valid{
            "100", "100.00", "100,00"
        };

        for (const auto& value : valid) {
            atm::test::TestAtmContext f;

            const auto card =
                atm::domain::CardNumber::parse("0000000000000001");

            f.storage.seed(atm::domain::Account(
                1, card, atm::domain::Money::uah(5000), true
            ));

            atm::presentation::Session session(
                card, atm::domain::CardKind::Client, 1
            );

            const auto option =
                f.withdrawal.quickAmounts().size() + 1;

            f.input.str(
                std::to_string(option) + "\n" + value + "\nno\n"
            );

            EXPECT_EQ(
                atm::presentation::withdrawDialog(f.ctx, session),
                Nav::Cancel
            );

            EXPECT_NE(
                f.output.str().find("Withdraw 100.00?"),
                std::string::npos
            );

            EXPECT_EQ(f.storage.commits, 0);
            EXPECT_EQ(
                f.storage.accounts().find(1)->balance(),
                atm::domain::Money::uah(5000)
            );
            EXPECT_TRUE(f.storage.journal().last(1).empty());
        }
    }

    TEST(M1Additional, WithdrawalEofBeforeConfirmationNoChanges)
    {
        atm::test::TestAtmContext f;

        const auto card =
            atm::domain::CardNumber::parse("0000000000000001");

        f.storage.seed(atm::domain::Account(
            1, card, atm::domain::Money::uah(5000), true
        ));

        atm::presentation::Session session(
            card, atm::domain::CardKind::Client, 1
        );

        const auto option =
            f.withdrawal.quickAmounts().size() + 1;

        f.input.str(std::to_string(option) + "\n100\n");

        EXPECT_EQ(
            atm::presentation::withdrawDialog(f.ctx, session),
            Nav::Exit
        );

        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            atm::domain::Money::uah(5000)
        );
        EXPECT_TRUE(f.storage.journal().last(1).empty());
    }

    TEST(M1Additional, PaymentInvalidAmountsNoChanges)
    {
        const std::vector<std::string> invalid{
            "abc",
            "100,123",
            "100.123",
            "100,5.0",
            "100.5,0",
            "999999999999999999999999999999"
        };

        for (const auto& value : invalid) {
            atm::test::TestAtmContext f;

            const auto card =
                atm::domain::CardNumber::parse("0000000000000001");

            f.storage.seed(atm::domain::Account(
                1, card, atm::domain::Money::uah(5000), true
            ));

            atm::presentation::Session session(
                card, atm::domain::CardKind::Client, 1
            );


            f.input.str("2\n1\n" + value + "\n");

            const auto result =
                atm::presentation::paymentDialog(f.ctx, session);

            EXPECT_EQ(result, Nav::Cancel);
            EXPECT_EQ(f.storage.commits, 0);
            EXPECT_EQ(
                f.storage.accounts().find(1)->balance(),
                atm::domain::Money::uah(5000)
            );
            EXPECT_TRUE(f.storage.journal().last(1).empty());
        }
    }

    TEST(M1Additional, PaymentDecimalAmountsCancelledNoChanges)
    {
        const std::vector<std::string> valid{
            "100", "100,50", "100.5", "100.50"
        };

        for (const auto& value : valid) {
            atm::test::TestAtmContext f;

            const auto card =
                atm::domain::CardNumber::parse("0000000000000001");

            f.storage.seed(atm::domain::Account(
                1, card, atm::domain::Money::uah(5000), true
            ));

            atm::presentation::Session session(
                card, atm::domain::CardKind::Client, 1
            );

            f.input.str("2\n1\n" + value + "\nno\n");

            const auto result =
                atm::presentation::paymentDialog(f.ctx, session);

            EXPECT_EQ(result, Nav::Cancel);

            const std::string expected =
                value == "100" ? "100.00" : "100.50";


            EXPECT_NE(
                f.output.str().find("Amount:"),
                std::string::npos
            );

            EXPECT_NE(
                f.output.str().find(expected),
                std::string::npos
            );


            EXPECT_EQ(f.storage.commits, 0);
            EXPECT_EQ(
                f.storage.accounts().find(1)->balance(),
                atm::domain::Money::uah(5000)
            );
            EXPECT_TRUE(f.storage.journal().last(1).empty());
        }
    }


    TEST(M1Additional, TransferInvalidAmountsNoChanges)
    {
        const std::vector<std::string> invalid{
            "abc",
            "100,123",
            "100.123",
            "100,5.0",
            "100.5,0",
            "0",
            "999999999999999999999999999999"
        };

        for (const auto& value : invalid) {
            atm::test::TestAtmContext f;

            const auto sender =
                atm::domain::CardNumber::parse("0000000000000001");

            const auto recipient =
                atm::domain::CardNumber::parse("0000000000000002");

            f.storage.seed(atm::domain::Account(
                1, sender, atm::domain::Money::uah(5000), true
            ));

            atm::presentation::Session session(
                sender, atm::domain::CardKind::Client, 1
            );

            const auto before =
                f.storage.accounts().find(1)->balance();

            f.input.str(recipient.str() + "\n" + value + "\n");

            EXPECT_EQ(
                atm::presentation::transferDialog(f.ctx, session),
                Nav::Cancel
            );

            EXPECT_EQ(f.storage.commits, 0);
            EXPECT_EQ(f.storage.accounts().find(1)->balance(), before);
            EXPECT_TRUE(f.storage.journal().last(1).empty());
        }
    }

    TEST(M1Additional, TransferInvalidRecipientNoChanges)
    {
        atm::test::TestAtmContext f;

        const auto sender =
            atm::domain::CardNumber::parse("0000000000000001");

        f.storage.seed(atm::domain::Account(
            1, sender, atm::domain::Money::uah(5000), true
        ));

        atm::presentation::Session session(
            sender, atm::domain::CardKind::Client, 1
        );

        f.input.str("abc\n");

        EXPECT_EQ(
            atm::presentation::transferDialog(f.ctx, session),
            Nav::Cancel
        );

        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            atm::domain::Money::uah(5000)
        );
        EXPECT_TRUE(f.storage.journal().last(1).empty());
    }

    TEST(M1Additional, TransferCancelAtRecipientNoChanges)
    {
        atm::test::TestAtmContext f;

        const auto sender =
            atm::domain::CardNumber::parse("0000000000000001");

        f.storage.seed(atm::domain::Account(
            1, sender, atm::domain::Money::uah(5000), true
        ));

        atm::presentation::Session session(
            sender, atm::domain::CardKind::Client, 1
        );

        f.input.str("cancel\n");

        EXPECT_EQ(
            atm::presentation::transferDialog(f.ctx, session),
            Nav::Cancel
        );

        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_TRUE(f.storage.journal().last(1).empty());
    }

    TEST(M1Additional, TransferEofAtAmountNoChanges)
    {
        atm::test::TestAtmContext f;

        const auto sender =
            atm::domain::CardNumber::parse("0000000000000001");

        const auto recipient =
            atm::domain::CardNumber::parse("0000000000000002");

        f.storage.seed(atm::domain::Account(
            1, sender, atm::domain::Money::uah(5000), true
        ));

        atm::presentation::Session session(
            sender, atm::domain::CardKind::Client, 1
        );

        f.input.str(recipient.str() + "\n");

        EXPECT_EQ(
            atm::presentation::transferDialog(f.ctx, session),
            Nav::Exit
        );

        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_TRUE(f.storage.journal().last(1).empty());
    }


} // namespace
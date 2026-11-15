#include <sstream>
#include <string>
#include <vector>

#include "presentation/console_ui.hpp"
#include "presentation/ui_types.hpp"
#include "support/mini_test.hpp"

namespace {

    using atm::presentation::ConsoleUi;
    using atm::presentation::Nav;
    using atm::presentation::Reply;

    // --- prompt -------------------------------------------------------

    TEST(M1ConsoleUi, PromptPreservesLeadingZeros) {
        std::istringstream input("0000123400\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.prompt("Enter card number:");

        EXPECT_EQ(reply.nav, Nav::Ok);
        EXPECT_EQ(reply.text, std::string("0000123400"));
        EXPECT_FALSE(reply.index.has_value());
    }

    TEST(M1ConsoleUi, PromptPreservesOriginalText) {
        std::istringstream input("  100,50  \n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.prompt("Enter amount:");

        EXPECT_EQ(reply.nav, Nav::Ok);
        EXPECT_EQ(reply.text, std::string("  100,50  "));
    }

    TEST(M1ConsoleUi, EmptyInputRetriesWithoutReturningEmptyReply) {
        std::istringstream input("\n   \n1234\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.prompt("Enter PIN:");

        EXPECT_EQ(reply.nav, Nav::Ok);
        EXPECT_EQ(reply.text, std::string("1234"));
        EXPECT_NE(
            output.str().find("Input cannot be empty."),
            std::string::npos
        );
    }

    TEST(M1ConsoleUi, PromptCancelAndExitCommands) {
        {
            std::istringstream input("  CANCEL  \n");
            std::ostringstream output;
            ConsoleUi ui(input, output);

            EXPECT_EQ(ui.prompt("Input:").nav, Nav::Cancel);
        }

        {
            std::istringstream input(" Exit \n");
            std::ostringstream output;
            ConsoleUi ui(input, output);

            EXPECT_EQ(ui.prompt("Input:").nav, Nav::Exit);
        }
    }

    TEST(M1ConsoleUi, PromptEofReturnsExit) {
        std::istringstream input("");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.prompt("Input:");

        EXPECT_EQ(reply.nav, Nav::Exit);
        EXPECT_EQ(reply.text, std::string(""));
        EXPECT_FALSE(reply.index.has_value());
    }

    // --- promptSecret -------------------------------------------------

    TEST(M1ConsoleUi, SecretPromptPreservesFourDigitPin) {
        std::istringstream input("0012\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.promptSecret("Enter PIN:");

        EXPECT_EQ(reply.nav, Nav::Ok);
        EXPECT_EQ(reply.text, std::string("0012"));
    }

    // --- choose -------------------------------------------------------

    TEST(M1ConsoleUi, ChooseReturnsZeroBasedIndex) {
        std::istringstream input("2\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Main menu:",
            { "Balance", "Withdraw", "Deposit" }
        );

        EXPECT_EQ(reply.nav, Nav::Ok);
        ASSERT_TRUE(reply.index.has_value());
        EXPECT_EQ(*reply.index, std::size_t{ 1 });
    }

    TEST(M1ConsoleUi, ChooseZeroReturnsCancel) {
        std::istringstream input("0\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Main menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Cancel);
        EXPECT_FALSE(reply.index.has_value());
    }

    TEST(M1ConsoleUi, ChooseRejectsInvalidOptionsAndRetries) {
        std::istringstream input("abc\n3\n-1\n1\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Main menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Ok);
        ASSERT_TRUE(reply.index.has_value());
        EXPECT_EQ(*reply.index, std::size_t{ 0 });

        EXPECT_NE(
            output.str().find("Invalid menu option."),
            std::string::npos
        );
    }

    // --- confirm ------------------------------------------------------

    TEST(M1ConsoleUi, ConfirmAcceptsYes) {
        std::istringstream input(" YES \n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        EXPECT_EQ(ui.confirm("Confirm operation?"), Nav::Ok);
    }

    TEST(M1ConsoleUi, ConfirmNoReturnsCancel) {
        std::istringstream input("n\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        EXPECT_EQ(ui.confirm("Confirm operation?"), Nav::Cancel);
    }

    TEST(M1ConsoleUi, ConfirmRetriesInvalidAnswerAndHandlesEof) {
        {
            std::istringstream input("maybe\ny\n");
            std::ostringstream output;
            ConsoleUi ui(input, output);

            EXPECT_EQ(ui.confirm("Confirm?"), Nav::Ok);

            EXPECT_NE(
                output.str().find("Please enter yes or no."),
                std::string::npos
            );
        }

        {
            std::istringstream input("");
            std::ostringstream output;
            ConsoleUi ui(input, output);

            EXPECT_EQ(ui.confirm("Confirm?"), Nav::Exit);
        }
    }






    TEST(M1ConsoleUi, ChooseExitCommandReturnsExit) {
        std::istringstream input("exit\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Client menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Exit);
        EXPECT_FALSE(reply.index.has_value());
    }

    TEST(M1ConsoleUi, ChooseEofReturnsExit) {
        std::istringstream input("");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Client menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Exit);
    }

    TEST(M1ConsoleUi, ChooseEmptyInputRetries) {
        std::istringstream input("\n  \n1\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const Reply reply = ui.choose(
            "Client menu:",
            { "Balance", "Withdraw" }
        );

        EXPECT_EQ(reply.nav, Nav::Ok);
        ASSERT_TRUE(reply.index.has_value());
        EXPECT_EQ(*reply.index, std::size_t{ 0 });
    }

    TEST(M1ConsoleUi, ConfirmCancelCommandReturnsCancel) {
        std::istringstream input("cancel\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        EXPECT_EQ(ui.confirm("Confirm operation?"), Nav::Cancel);
    }

    TEST(M1ConsoleUi, ConfirmExitCommandReturnsExit) {
        std::istringstream input("exit\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        EXPECT_EQ(ui.confirm("Confirm operation?"), Nav::Exit);
    }

    TEST(M1ConsoleUi, ConfirmEmptyInputRetries) {
        std::istringstream input("\n  \nyes\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        EXPECT_EQ(ui.confirm("Confirm operation?"), Nav::Ok);
    }


} // namespace

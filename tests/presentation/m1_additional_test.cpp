#include "support/mini_test.hpp"

#include "presentation/console_ui.hpp"
#include "presentation/ui_types.hpp"

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

} // namespace
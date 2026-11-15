#include "support/mini_test.hpp"
#include "presentation/console_ui.hpp"

#include <sstream>
#include <string>

namespace {

    using atm::presentation::ConsoleUi;
    using atm::presentation::Nav;

    TEST(SEC1ConsoleUi, SecretPromptDoesNotEchoPin) {
        std::istringstream input("7392\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const auto reply = ui.promptSecret("Enter PIN:");

        EXPECT_EQ(reply.nav, Nav::Ok);
        EXPECT_EQ(reply.text, std::string("7392"));

        EXPECT_EQ(output.str().find("7392"), std::string::npos);
    }

    TEST(SEC1ConsoleUi, SecretPromptPreservesLeadingZeros) {
        std::istringstream input("0037\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        const auto reply = ui.promptSecret("Enter PIN:");

        EXPECT_EQ(reply.nav, Nav::Ok);
        EXPECT_EQ(reply.text, std::string("0037"));

        EXPECT_EQ(output.str().find("0037"), std::string::npos);
    }

    TEST(SEC1ConsoleUi, ErrorDoesNotExposePreviousPin) {
        std::istringstream input("7392\n");
        std::ostringstream output;
        ConsoleUi ui(input, output);

        ui.promptSecret("Enter PIN:");
        ui.showError("Incorrect PIN.");

        EXPECT_EQ(output.str().find("7392"), std::string::npos);
        EXPECT_NE(
            output.str().find("Incorrect PIN."),
            std::string::npos
        );
    }

} // namespace

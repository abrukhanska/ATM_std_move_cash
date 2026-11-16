
#include "presentation/console_ui.hpp"

#include <cctype>
#include <ios>
#include <string>

namespace atm::presentation {

    namespace {

        constexpr const char* PURPLE = "\033[95m";
        constexpr const char* WHITE = "\033[38;2;248;250;252m";
        constexpr const char* GREEN = "\033[38;2;134;239;172m";
        constexpr const char* RED = "\033[91m";
        constexpr const char* PINK = "\033[95m";
        constexpr const char* LAVENDER = "\033[38;2;165;180;252m";
        constexpr const char* SOFT_RED = "\033[38;2;251;146;160m";
        constexpr const char* RESET = "\033[0m";

        constexpr int WIDTH = 46;

        void printBorder(std::ostream& out) {
            out << PURPLE << "+"
                << std::string(WIDTH, '-')
                << "+" << RESET << '\n';
        }

        void printLine(std::ostream& out, const std::string& text) {
            out << PURPLE << "| " << RESET;

            const std::size_t colon = text.find(':');

            if (colon != std::string::npos) {
                const std::string label = text.substr(0, colon + 1);
                const std::string value = text.substr(colon + 1);

                if (label == "Recipient:" ||
                    label == "Card:" ||
                    label == "Amount:" ||
                    label == "Total:") {

                    out << LAVENDER << label << RESET;

                    if (label == "Amount:" || label == "Total:") {
                        out << GREEN << value << RESET;
                    }
                    else {
                        out << WHITE << value << RESET;
                    }
                }
                else {
                    out << WHITE << text << RESET;
                }
            }
            else {
                out << WHITE << text << RESET;
            }

            if (text.size() < WIDTH - 2) {
                out << std::string(WIDTH - 2 - text.size(), ' ');
            }

            out << PURPLE << " |" << RESET << '\n';
        }

        void printMenuItem(
            std::ostream& out,
            const std::string& number,
            const std::string& title,
            bool isCancel = false
        ) {
            const std::string text = number + " " + title;

            out << PURPLE << "| " << RESET;

            if (isCancel) {
                out << SOFT_RED << number << RESET
                    << WHITE << " " << title << RESET;
            }
            else {
                out << LAVENDER << number << RESET
                    << WHITE << " " << title << RESET;
            }

            if (text.size() < WIDTH - 2) {
                out << std::string(WIDTH - 2 - text.size(), ' ');
            }

            out << PURPLE << " |" << RESET << '\n';
        }

        std::string trim(const std::string& text) {
            const std::size_t first = text.find_first_not_of(" \t\r\n");

            if (first == std::string::npos) {
                return "";
            }

            const std::size_t last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        std::string lowerAscii(std::string text) {
            for (char& ch : text) {
                ch = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(ch))
                    );
            }

            return text;
        }

        void ensureOutput(std::ostream& out) {
            out.flush();

            if (!out) {
                throw std::ios_base::failure("Console output failed.");
            }
        }

    } // namespace

    ConsoleUi::ConsoleUi(std::istream& in, std::ostream& out)
        : in_(in), out_(out) {
    }

    void ConsoleUi::showScreen(
        const std::string& title,
        const std::vector<std::string>& lines
    ) {
        out_ << '\n';

        printBorder(out_);
        printLine(out_, "             std::move(cash)");
        printBorder(out_);

        printLine(out_, title);

        for (const auto& line : lines) {
            printLine(out_, line);
        }

        printBorder(out_);
        out_ << '\n';

        ensureOutput(out_);
    }

    void ConsoleUi::showError(const std::string& message) {
        out_ << RED << "\n  [ERROR] " << message
            << RESET << "\n\n";

        ensureOutput(out_);
    }

    Reply ConsoleUi::prompt(const std::string& message) {
        while (true) {
            out_ << '\n';

            printBorder(out_);
            printLine(out_, "             std::move(cash)");
            printBorder(out_);
            printLine(out_, message);
            printBorder(out_);

            out_ << "  " << LAVENDER << "> " << RESET;

            ensureOutput(out_);

            std::string input;

            if (!std::getline(in_, input)) {
                return { Nav::Exit, "", std::nullopt };
            }

            const std::string command = lowerAscii(trim(input));

            if (command == "cancel") {
                return { Nav::Cancel, "", std::nullopt };
            }

            if (command == "exit") {
                return { Nav::Exit, "", std::nullopt };
            }

            if (trim(input).empty()) {
                showError("Input cannot be empty.");
                continue;
            }

            return { Nav::Ok, input, std::nullopt };
        }
    }

    Reply ConsoleUi::promptSecret(const std::string& message) {
        return prompt(message);
    }

    Reply ConsoleUi::choose(
        const std::string& title,
        const std::vector<std::string>& options
    ) {
        while (true) {
            out_ << '\n';
            printBorder(out_);
            printLine(out_, title);
            printBorder(out_);

            for (std::size_t i = 0; i < options.size(); ++i) {
                printMenuItem(
                    out_,
                    "[" + std::to_string(i + 1) + "]",
                    options[i]
                );
            }

            printMenuItem(out_, "[0]", "Cancel", true);
            printBorder(out_);

            ensureOutput(out_);

            const Reply reply = prompt("Select an option:");

            if (reply.nav != Nav::Ok) {
                return reply;
            }

            const std::string input = trim(reply.text);

            if (input == "0") {
                return { Nav::Cancel, "", std::nullopt };
            }

            try {
                std::size_t pos = 0;
                const unsigned long selected = std::stoul(input, &pos);

                if (pos == input.size()
                    && selected >= 1
                    && selected <= options.size()) {
                    return {
                        Nav::Ok,
                        "",
                        static_cast<std::size_t>(selected - 1)
                    };
                }
            }
            catch (const std::exception&) {
            }

            showError("Invalid menu option.");
        }
    }

    Nav ConsoleUi::confirm(const std::string& message) {
        while (true) {
            const Reply reply = prompt(message + " (yes/no):");

            if (reply.nav != Nav::Ok) {
                return reply.nav;
            }

            const std::string answer = lowerAscii(trim(reply.text));

            if (answer == "yes" || answer == "y") {
                return Nav::Ok;
            }

            if (answer == "no" || answer == "n") {
                return Nav::Cancel;
            }

            showError("Please enter yes or no.");
        }
    }

} // namespace atm::presentation


#include "presentation/receipt_printer.hpp"

#include "domain/enums.hpp"

#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <ios>

namespace atm::presentation {

    namespace {

        constexpr const char* BORDER = "\033[95m";
        constexpr const char* PURPLE = "\033[38;2;232;121;249m";
        constexpr const char* LAVENDER = "\033[38;2;165;180;252m";
        constexpr const char* WHITE = "\033[38;2;248;250;252m";
        constexpr const char* GREEN = "\033[38;2;134;239;172m";
        constexpr const char* RESET = "\033[0m";

        constexpr int WIDTH = 46;

        void printBorder(std::ostream& out) {
            out << BORDER << "+"
                << std::string(WIDTH, '-')
                << "+" << RESET << '\n';
        }

        void printLine(std::ostream& out, const std::string& text,
            const char* color = WHITE) {
            out << BORDER << "| " << RESET
                << color << text << RESET;

            if (text.size() < WIDTH - 2) {
                out << std::string(WIDTH - 2 - text.size(), ' ');
            }

            out << BORDER << " |" << RESET << '\n';
        }

        void printField(std::ostream& out, const std::string& label,
            const std::string& value, const char* valueColor = WHITE) {
            const std::string text = label + value;

            out << BORDER << "| " << RESET
                << LAVENDER << label << RESET
                << valueColor << value << RESET;

            if (text.size() < WIDTH - 2) {
                out << std::string(WIDTH - 2 - text.size(), ' ');
            }

            out << BORDER << " |" << RESET << '\n';
        }

    } // namespace

    ConsoleReceiptPrinter::ConsoleReceiptPrinter(std::ostream& out)
        : out_(out) {
    }

    void ConsoleReceiptPrinter::print(const atm::domain::JournalEntry& entry) {
        out_ << '\n';

        printBorder(out_);
        printLine(out_, "             std::move(cash)", PURPLE);
        printBorder(out_);

        printLine(out_, "TRANSACTION RECEIPT", WHITE);
        printLine(out_, "");

        printField(out_, "Transaction ID: ", std::to_string(entry.id));
        printField(out_, "Date: ", entry.localDay);
        
        const std::time_t timestamp = static_cast<std::time_t>(entry.atUtc);
        const std::tm* local = std::localtime(&timestamp);

        if (local != nullptr) {
            std::ostringstream timeStream;
            timeStream << std::put_time(local, "%H:%M:%S");
            printField(out_, "Time (local): ", timeStream.str());
        }

        printField(out_, "Operation: ", std::string(atm::domain::toString(entry.kind)));
        printField(out_, "Card: ", entry.actor.masked());
        printField(out_, "Amount: ", entry.amount.format() + " UAH", GREEN);

        if (entry.kind == domain::EntryKind::Withdrawal ||
            entry.kind == domain::EntryKind::Deposit ||
            entry.kind == domain::EntryKind::CashLoaded ||
            entry.kind == domain::EntryKind::CashUnloaded) {

            bool first = true;

            for (const auto denomination : domain::kAllDenominations) {
                const auto count = entry.cash.count(denomination);

                if (count > 0) {
                    const std::string banknotes =
                        std::to_string(domain::denominationUah(denomination))
                        + " UAH x " + std::to_string(count);

                    printField(
                        out_,
                        first ? "Banknotes: " : "           ",
                        banknotes,
                        WHITE
                    );

                    first = false;
                }
            }
        }





        if (entry.account.has_value()) {
            printField(out_, "Account ID: ", std::to_string(*entry.account));
        }

        if (entry.target.has_value()) {
            printField(out_, "Target account: ", std::to_string(*entry.target));
        }

        if (entry.subjectCard.has_value()) {
            printField(out_, "Subject card: ", entry.subjectCard->masked());
        }


        printLine(out_, "");
        printLine(out_, "Thank you for using our ATM!", WHITE);

        printBorder(out_);
        out_ << '\n';

        out_.flush();

        if (!out_) {
            throw std::ios_base::failure("Receipt printing failed.");
        }
    }


} // namespace atm::presentation



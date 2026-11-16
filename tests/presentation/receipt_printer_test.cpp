#include "presentation/receipt_printer.hpp"

#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/journal_entry.hpp"
#include "domain/money.hpp"

#include "support/mini_test.hpp"

#include <sstream>
#include <string>

namespace {

    using atm::domain::CardNumber;
    using atm::domain::JournalEntry;
    using atm::presentation::ConsoleReceiptPrinter;

    TEST(R1ReceiptPrinter, PrintsTransactionDetails) {
        std::ostringstream output;
        ConsoleReceiptPrinter printer(output);

        JournalEntry entry;
        entry.id = 42;
        entry.localDay = "2026-11-15";
        entry.actor = CardNumber::parse("1234567890123456");
        entry.amount = atm::domain::Money::uah(500);
        entry.account = 7;
        entry.target = 8;

        printer.print(entry);

        const std::string text = output.str();

        EXPECT_NE(text.find("Transaction ID: "), std::string::npos);
        EXPECT_NE(text.find("42"), std::string::npos);

        EXPECT_NE(text.find("Date: "), std::string::npos);
        EXPECT_NE(text.find("2026-11-15"), std::string::npos);

        EXPECT_NE(text.find("Amount: "), std::string::npos);
        EXPECT_NE(text.find("500.00 UAH"), std::string::npos);

        EXPECT_NE(text.find("Account ID: "), std::string::npos);
        EXPECT_NE(text.find("7"), std::string::npos);

        EXPECT_NE(text.find("Target account: "), std::string::npos);
        EXPECT_NE(text.find("8"), std::string::npos);
    }

    TEST(R1ReceiptPrinter, MasksCardNumbers) {
        std::ostringstream output;
        ConsoleReceiptPrinter printer(output);

        JournalEntry entry;
        entry.actor = CardNumber::parse("1234567890123456");
        entry.subjectCard = CardNumber::parse("9876543210987654");

        printer.print(entry);

        const std::string text = output.str();

        EXPECT_NE(
            text.find(entry.actor.masked()),
            std::string::npos
        );
        EXPECT_NE(
            text.find(entry.subjectCard->masked()),
            std::string::npos
        );

        EXPECT_EQ(
            text.find(entry.actor.str()),
            std::string::npos
        );
        EXPECT_EQ(
            text.find(entry.subjectCard->str()),
            std::string::npos
        );
    }

    TEST(R1ReceiptPrinter, OmitsAbsentOptionalFields) {
        std::ostringstream output;
        ConsoleReceiptPrinter printer(output);

        JournalEntry entry;
        entry.actor = CardNumber::parse("1234567890123456");

        printer.print(entry);

        const std::string text = output.str();

        EXPECT_EQ(text.find("Account ID:"), std::string::npos);
        EXPECT_EQ(text.find("Target account:"), std::string::npos);
        EXPECT_EQ(text.find("Subject card:"), std::string::npos);
    }


    TEST(SEC1ReceiptPrinter, DoesNotExposeSensitivePinData) {
        std::ostringstream output;
        ConsoleReceiptPrinter printer(output);

        JournalEntry entry;
        entry.actor = CardNumber::parse("1234567890123456");
        entry.requisite = "test-requisite";

        printer.print(entry);

        const std::string text = output.str();

        EXPECT_EQ(text.find("pin_hash"), std::string::npos);
        EXPECT_EQ(text.find("PIN:"), std::string::npos);
        EXPECT_EQ(text.find("test-requisite"), std::string::npos);

        EXPECT_EQ(text.find(entry.actor.str()), std::string::npos);
        EXPECT_NE(text.find(entry.actor.masked()), std::string::npos);
    }


} // namespace

#include "support/mini_test.hpp"
#include "presentation/receipt_printer.hpp"

#include <sstream>
#include <string>

namespace {

    using namespace atm::presentation;
    using namespace atm::domain;

    // X4: Printing to a failed stream must not crash
    // when stream exceptions are disabled.
    TEST(X4Receipt, HandlesFailedOutputStream) {
        std::ostringstream output;
        output.setstate(std::ios::badbit);

        ConsoleReceiptPrinter printer(output);
        JournalEntry entry;

        EXPECT_NO_THROW(printer.print(entry));
        EXPECT_TRUE(output.fail());
    }

    // X4: A failed output stream must not produce
    // a partial or duplicate receipt.
    TEST(X4Receipt, FailedOutputDoesNotProduceReceipt) {
        std::ostringstream output;
        output.setstate(std::ios::badbit);

        ConsoleReceiptPrinter printer(output);
        JournalEntry entry;

        EXPECT_NO_THROW(printer.print(entry));
        EXPECT_TRUE(output.str().empty());
        EXPECT_TRUE(output.fail());
    }

} // namespace


#include "support/mini_test.hpp"
#include "presentation/receipt_printer.hpp"
#include "support/test_atm_context.hpp"
#include "presentation/dialogs.hpp"
#include "presentation/session.hpp"
#include "domain/account.hpp"
#include "domain/card_number.hpp"
#include "domain/banknote_set.hpp"
#include "domain/enums.hpp"
#include "domain/card.hpp"

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

        EXPECT_THROW(printer.print(entry), std::ios_base::failure);
        EXPECT_TRUE(output.fail());
    }

    // X4: A failed output stream must not produce
    // a partial or duplicate receipt.
    TEST(X4Receipt, FailedOutputDoesNotProduceReceipt) {
        std::ostringstream output;
        output.setstate(std::ios::badbit);

        ConsoleReceiptPrinter printer(output);
        JournalEntry entry;

        EXPECT_THROW(printer.print(entry), std::ios_base::failure);
        EXPECT_TRUE(output.str().empty());
        EXPECT_TRUE(output.fail());
    }



    TEST(X4Receipt, WithdrawalReceiptFailureDoesNotRepeatTransaction) {
        atm::test::TestAtmContext f;

        const auto card =
            CardNumber::parse("0000000000000001");

        // Register the client card in storage.
        f.storage.seed(Card(
            card,
            "Test Client",
            "test-pin-hash",
            CardKind::Client
        ));

        f.storage.seed(Account(
            1, card, Money::uah(5000), true
        ));


        // Give the ATM enough cash for the withdrawal.
        BanknoteSet cash;
        cash = cash.with(Denomination::UAH100, 10);
        f.storage.seed(cash);

        Session session(card, CardKind::Client, 1);

        // Custom amount, confirm withdrawal, request receipt.
        const auto customOption =
            f.withdrawal.quickAmounts().size() + 1;

        f.input.str(
            std::to_string(customOption) +
            "\n100\nyes\nyes\n"
        );

        // Simulate a failed receipt output stream.
        // The UI uses the same stream, so we must test
        // the financial state independently of its output.
        f.output.setstate(std::ios::badbit);

        const Nav result = withdrawDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Ok);

        // The withdrawal must be committed exactly once.
        EXPECT_EQ(f.storage.commits, 1);

        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            Money::uah(4900)
        );

        const auto journal = f.storage.journal().last(2);

        EXPECT_EQ(journal.size(), std::size_t(1));

        if (!journal.empty()) {
            EXPECT_EQ(journal[0].kind, EntryKind::Withdrawal);
            EXPECT_EQ(journal[0].amount, Money::uah(100));
        }

        EXPECT_TRUE(f.output.fail());
    }


} // namespace


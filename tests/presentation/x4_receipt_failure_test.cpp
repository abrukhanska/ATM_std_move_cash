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
#include "presentation/atm_context.hpp"

#include <ios>
#include <sstream>
#include <string>

namespace {

    using namespace atm::presentation;
    using namespace atm::domain;




    class FailingReceiptPrinter final : public IReceiptPrinter {
    public:
        int attempts = 0;

        void print(const JournalEntry&) override {
            ++attempts;
            throw std::ios_base::failure("Simulated printer failure");
        }
    };



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




    TEST(X4Receipt, BrokenConsolePreventsWithdrawal) {
        atm::test::TestAtmContext f;

        const auto card = CardNumber::parse("0000000000000001");

        f.storage.seed(Card(
            card,
            "Test Client",
            "test-pin-hash",
            CardKind::Client
        ));

        f.storage.seed(Account(
            1, card, Money::uah(5000), true
        ));

        BanknoteSet cash;
        cash = cash.with(Denomination::UAH100, 10);
        f.storage.seed(cash);

        Session session(card, CardKind::Client, 1);

        const auto customOption =
            f.withdrawal.quickAmounts().size() + 1;

        f.input.str(
            std::to_string(customOption) +
            "\n100\nyes\nyes\n"
        );

        // Break the console before the operation starts.
        f.output.setstate(std::ios::badbit);

        EXPECT_THROW(
            withdrawDialog(f.ctx, session),
            std::ios_base::failure
        );

        // No financial operation must be executed.
        EXPECT_EQ(f.storage.commits, 0);

        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            Money::uah(5000)
        );

        const auto journal = f.storage.journal().last(2);
        EXPECT_EQ(journal.size(), std::size_t(0));

        EXPECT_TRUE(f.output.fail());
    }


    TEST(X4Receipt, PrinterFailureAfterCommitEndsSession) {
        atm::test::TestAtmContext f;

        const auto card = CardNumber::parse("0000000000000001");

        f.storage.seed(Card(
            card,
            "Test Client",
            "test-pin-hash",
            CardKind::Client
        ));

        f.storage.seed(Account(
            1, card, Money::uah(5000), true
        ));

        BanknoteSet cash;
        cash = cash.with(Denomination::UAH100, 10);
        f.storage.seed(cash);

        Session session(card, CardKind::Client, 1);

        const auto customOption =
            f.withdrawal.quickAmounts().size() + 1;

        f.input.str(
            std::to_string(customOption) +
            "\n100\nyes\nyes\n"
        );

        FailingReceiptPrinter failingPrinter;

        // Same services and working UI, but a failing printer.
        AtmContext ctx(
            f.auth,
            f.accounts,
            f.withdrawal,
            f.deposit,
            f.transfer,
            f.payment,
            f.cash,
            f.monitoring,
            f.cardAdministration,
            f.device,
            f.ui,
            failingPrinter,
            f.support,
            f.ctx.clock
        );

        const Nav result = withdrawDialog(ctx, session);

        // A printer failure must end the client session.
        EXPECT_EQ(result, Nav::Exit);

        // Printing must be attempted only once.
        EXPECT_EQ(failingPrinter.attempts, 1);

        // The completed withdrawal must not be repeated.
        EXPECT_EQ(f.storage.commits, 1);

        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            Money::uah(4900)
        );

        const auto remainingCash = f.storage.cash().load();

        EXPECT_EQ(
            remainingCash.count(Denomination::UAH100),
            9
        );

        const auto journal = f.storage.journal().last(2);

        EXPECT_EQ(journal.size(), std::size_t(1));

        if (!journal.empty()) {
            EXPECT_EQ(journal[0].kind, EntryKind::Withdrawal);
            EXPECT_EQ(journal[0].amount, Money::uah(100));
            EXPECT_EQ(journal[0].cash.count(Denomination::UAH100), 1);
        }

        // Console output remains healthy.
        EXPECT_FALSE(f.output.fail());
    }



    TEST(X4Receipt, DepositPrinterFailureAfterCommit) {
        atm::test::TestAtmContext f;

        const auto card = CardNumber::parse("0000000000000001");

        f.storage.seed(Card(
            card, "Test Client", "test-pin-hash", CardKind::Client
        ));
        f.storage.seed(Account(
            1, card, Money::uah(5000), true
        ));

        Session session(card, CardKind::Client, 1);

        // One 100 UAH banknote, other denominations skipped.
        f.input.str("1\n0\n0\n0\n0\nyes\nyes\n");

        FailingReceiptPrinter failingPrinter;

        AtmContext ctx(
            f.auth, f.accounts, f.withdrawal, f.deposit,
            f.transfer, f.payment, f.cash, f.monitoring,
            f.cardAdministration, f.device, f.ui,
            failingPrinter, f.support, f.ctx.clock
        );

        const Nav result = depositDialog(ctx, session);

        EXPECT_EQ(result, Nav::Exit);
        EXPECT_EQ(failingPrinter.attempts, 1);
        EXPECT_EQ(f.storage.commits, 1);

        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            Money::uah(5100)
        );

        const auto journal = f.storage.journal().last(2);
        EXPECT_EQ(journal.size(), std::size_t(1));

        if (!journal.empty()) {
            EXPECT_EQ(journal[0].kind, EntryKind::Deposit);
            EXPECT_EQ(journal[0].amount, Money::uah(100));
        }

        EXPECT_FALSE(f.output.fail());
    }

    TEST(X4Receipt, TransferPrinterFailureAfterCommit) {
        atm::test::TestAtmContext f;

        const auto sender = CardNumber::parse("0000000000000001");
        const auto recipient = CardNumber::parse("0000000000000002");

        f.storage.seed(Card(
            sender, "Sender", "test-pin-hash", CardKind::Client
        ));
        f.storage.seed(Card(
            recipient, "Recipient", "test-pin-hash", CardKind::Client
        ));

        f.storage.seed(Account(
            1, sender, Money::uah(5000), true
        ));
        f.storage.seed(Account(
            2, recipient, Money::uah(1000), true
        ));

        Session session(sender, CardKind::Client, 1);

        f.input.str(recipient.str() + "\n100\nyes\nyes\n");

        FailingReceiptPrinter failingPrinter;

        AtmContext ctx(
            f.auth, f.accounts, f.withdrawal, f.deposit,
            f.transfer, f.payment, f.cash, f.monitoring,
            f.cardAdministration, f.device, f.ui,
            failingPrinter, f.support, f.ctx.clock
        );

        const Nav result = transferDialog(ctx, session);

        EXPECT_EQ(result, Nav::Exit);
        EXPECT_EQ(failingPrinter.attempts, 1);
        EXPECT_EQ(f.storage.commits, 1);

        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            Money::uah(4900)
        );
        EXPECT_EQ(
            f.storage.accounts().find(2)->balance(),
            Money::uah(1100)
        );

        const auto journal = f.storage.journal().last(2);
        EXPECT_EQ(journal.size(), std::size_t(1));

        if (!journal.empty()) {
            EXPECT_EQ(journal[0].kind, EntryKind::Transfer);
            EXPECT_EQ(journal[0].amount, Money::uah(100));
        }

        EXPECT_FALSE(f.output.fail());
    }

    TEST(X4Receipt, PaymentPrinterFailureAfterCommit) {
        atm::test::TestAtmContext f;

        const auto card = CardNumber::parse("0000000000000001");

        f.storage.seed(Card(
            card, "Test Client", "test-pin-hash", CardKind::Client
        ));
        f.storage.seed(Account(
            1, card, Money::uah(5000), true
        ));

        Session session(card, CardKind::Client, 1);

        // Charity category, first payee, 100 UAH.
        f.input.str("2\n1\n100\nyes\nyes\n");

        FailingReceiptPrinter failingPrinter;

        AtmContext ctx(
            f.auth, f.accounts, f.withdrawal, f.deposit,
            f.transfer, f.payment, f.cash, f.monitoring,
            f.cardAdministration, f.device, f.ui,
            failingPrinter, f.support, f.ctx.clock
        );

        const Nav result = paymentDialog(ctx, session);

        EXPECT_EQ(result, Nav::Exit);
        EXPECT_EQ(failingPrinter.attempts, 1);
        EXPECT_EQ(f.storage.commits, 1);

        EXPECT_EQ(
            f.storage.accounts().find(1)->balance(),
            Money::uah(4900)
        );

        const auto journal = f.storage.journal().last(2);
        EXPECT_EQ(journal.size(), std::size_t(1));

        if (!journal.empty()) {
            EXPECT_EQ(journal[0].kind, EntryKind::Payment);
            EXPECT_EQ(journal[0].amount, Money::uah(100));
        }

        EXPECT_FALSE(f.output.fail());
    }


} // namespace


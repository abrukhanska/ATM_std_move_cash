#include "support/mini_test.hpp"
#include "support/test_atm_context.hpp"

#include "presentation/dialogs.hpp"
#include "presentation/session.hpp"

#include "domain/account.hpp"
#include "domain/banknote_set.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/money.hpp"

#include <string>

namespace {

    using namespace atm;
    using namespace atm::presentation;

    const auto testCard =
        domain::CardNumber::parse("4000000000000001");

    // Prepare a client account and ATM cash for withdrawal.
    void prepareWithdrawal(test::TestAtmContext& fixture) {
        fixture.storage.seed(domain::Card(
            testCard,
            "Alice",
            fixture.pinHasher.hash("1234"),
            domain::CardKind::Client
        ));

        fixture.storage.seed(domain::Account(
            1,
            testCard,
            domain::Money::uah(20000),
            true
        ));

        domain::BanknoteSet notes;
        notes = notes.with(domain::Denomination::UAH100, 20);
        fixture.storage.seed(notes);
    }

    // Create an authenticated client session.
    Session makeSession() {
        return Session(
            testCard,
            domain::CardKind::Client,
            domain::AccountId{ 1 }
        );
    }

    // R1: An accepted receipt must be printed after withdrawal.
    TEST(R1Dialog, AcceptedReceiptIsPrinted) {
        test::TestAtmContext fixture;
        prepareWithdrawal(fixture);

        // Quick amount, confirm withdrawal, accept receipt.
        fixture.input.str("1\ny\ny\n");

        auto session = makeSession();

        EXPECT_EQ(withdrawDialog(fixture.ctx, session), Nav::Ok);
        EXPECT_EQ(fixture.storage.commits, 1);

        EXPECT_NE(
            fixture.output.str().find("TRANSACTION RECEIPT"),
            std::string::npos
        );
    }

    // R1: Declining the receipt must not cancel withdrawal.
    TEST(R1Dialog, DeclinedReceiptIsNotPrinted) {
        test::TestAtmContext fixture;
        prepareWithdrawal(fixture);

        // Quick amount, confirm withdrawal, decline receipt.
        fixture.input.str("1\ny\nn\n");

        auto session = makeSession();

        EXPECT_EQ(withdrawDialog(fixture.ctx, session), Nav::Ok);
        EXPECT_EQ(fixture.storage.commits, 1);

        EXPECT_EQ(
            fixture.output.str().find("TRANSACTION RECEIPT"),
            std::string::npos
        );
    }

    // R1: Cancelling receipt printing must not undo withdrawal.
    TEST(R1Dialog, CancelledReceiptDoesNotUndoWithdrawal) {
        test::TestAtmContext fixture;
        prepareWithdrawal(fixture);

        // Quick amount, confirm withdrawal, cancel receipt.
        fixture.input.str("1\ny\ncancel\n");

        auto session = makeSession();

        EXPECT_EQ(withdrawDialog(fixture.ctx, session), Nav::Ok);
        EXPECT_EQ(fixture.storage.commits, 1);

        EXPECT_EQ(
            fixture.output.str().find("TRANSACTION RECEIPT"),
            std::string::npos
        );
    }

} // namespace

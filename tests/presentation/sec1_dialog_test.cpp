#include "support/mini_test.hpp"
#include "support/test_atm_context.hpp"

#include "presentation/dialogs.hpp"
#include "presentation/session.hpp"
#include "presentation/atm_state.hpp"

#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "presentation/dialogs.hpp"

#include <string>

namespace {

    using namespace atm::presentation;
    using namespace atm::domain;

    const CardNumber cardNumber =
        CardNumber::parse("4000000000000001");

    void seedClient(atm::test::TestAtmContext& f) {
        f.storage.seed(Card(
            cardNumber,
            "Alice",
            "stored-secret-hash",
            CardKind::Client
        ));
    }

    TEST(SEC1Dialog, InvalidPinFormatDoesNotExposeInput) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("12345\ncancel\n");

        auto next = loginDialog(f.ctx, cardNumber);

        EXPECT_TRUE(next == nullptr);
        EXPECT_EQ(f.storage.commits, 0);

        EXPECT_NE(
            f.output.str().find("Invalid PIN format."),
            std::string::npos
        );
        EXPECT_EQ(
            f.output.str().find("12345"),
            std::string::npos
        );
        EXPECT_EQ(
            f.output.str().find("stored-secret-hash"),
            std::string::npos
        );
    }

    TEST(SEC1Dialog, InvalidPinWithLettersIsNotExposed) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("12ab\ncancel\n");

        auto next = loginDialog(f.ctx, cardNumber);

        EXPECT_TRUE(next == nullptr);
        EXPECT_EQ(f.storage.commits, 0);
        EXPECT_EQ(
            f.output.str().find("12ab"),
            std::string::npos
        );
    }

    TEST(SEC1Dialog, ChangePinMismatchDoesNotExposePins) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("1234\n5678\n8765\ncancel\n");

        Session session(cardNumber, CardKind::Client);

        const Nav result = changePinDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Cancel);
        EXPECT_EQ(f.storage.commits, 0);

        EXPECT_NE(
            f.output.str().find("New PINs do not match."),
            std::string::npos
        );

        EXPECT_EQ(f.output.str().find("1234"), std::string::npos);
        EXPECT_EQ(f.output.str().find("5678"), std::string::npos);
        EXPECT_EQ(f.output.str().find("8765"), std::string::npos);
        EXPECT_EQ(
            f.output.str().find("stored-secret-hash"),
            std::string::npos
        );
    }

    TEST(SEC1Dialog, InvalidNewPinDoesNotExposeInput) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("1234\n99999\n99999\ncancel\n");

        Session session(cardNumber, CardKind::Client);

        const Nav result = changePinDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Cancel);
        EXPECT_EQ(f.storage.commits, 0);

        EXPECT_NE(
            f.output.str().find("PIN must contain exactly 4 digits."),
            std::string::npos
        );

        EXPECT_EQ(f.output.str().find("1234"), std::string::npos);
        EXPECT_EQ(f.output.str().find("99999"), std::string::npos);
    }

    TEST(SEC1Dialog, CancelledChangeDoesNotExposePin) {
        atm::test::TestAtmContext f;
        seedClient(f);

        f.input.str("1234\ncancel\n");

        Session session(cardNumber, CardKind::Client);

        const Nav result = changePinDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Cancel);
        EXPECT_EQ(f.storage.commits, 0);

        EXPECT_EQ(f.output.str().find("1234"), std::string::npos);
        EXPECT_EQ(
            f.output.str().find("stored-secret-hash"),
            std::string::npos
        );
    }



    TEST(SEC1Dialog, SupportRequestDoesNotExposeSensitiveData) {
        atm::test::TestAtmContext f;

        const CardNumber number =
            CardNumber::parse("4000000000000001");

        f.storage.seed(Card(
            number,
            "Alice",
            "stored-secret-hash",
            CardKind::Client
        ));

        f.input.str("1\nyes\n");

        Session session(number, CardKind::Client);

        const Nav result = supportDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Ok);
        EXPECT_EQ(f.support.sendCount, 1);

        const std::string& message = f.support.lastMessage;

        EXPECT_NE(
            message.find("Transaction problem"),
            std::string::npos
        );

        EXPECT_EQ(message.find("1234"), std::string::npos);
        EXPECT_EQ(message.find("stored-secret-hash"), std::string::npos);
        EXPECT_EQ(message.find(number.str()), std::string::npos);
        EXPECT_EQ(message.find("Alice"), std::string::npos);

        EXPECT_EQ(f.storage.commits, 0);
    }


} // namespace

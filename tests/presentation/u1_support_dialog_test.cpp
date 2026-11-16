#include "support/mini_test.hpp"
#include "support/test_atm_context.hpp"

#include "presentation/dialogs.hpp"
#include "presentation/session.hpp"
#include "presentation/support_channel.hpp"

#include "domain/card_number.hpp"
#include "domain/enums.hpp"

#include <stdexcept>
#include <string_view>

namespace {

    using namespace atm::presentation;
    using namespace atm::domain;

    Session makeSession() {
        return Session(
            CardNumber::parse("4000000000000001"),
            CardKind::Client
        );
    }

    struct FailingSupportChannel final : ISupportChannel {
        int attempts = 0;

        void send(std::string_view) override {
            ++attempts;
            throw std::runtime_error("Support unavailable");
        }
    };

    TEST(U1Dialog, ConfirmedRequestIsSentOnce) {
        atm::test::TestAtmContext f;
        f.input.str("1\nyes\n");

        auto session = makeSession();

        const Nav result = supportDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Ok);
        EXPECT_EQ(f.support.sendCount, 1);
        EXPECT_TRUE(
            f.support.lastMessage.find("Transaction problem")
            != std::string::npos
        );
        EXPECT_TRUE(
            f.output.str().find("Your support request has been sent.")
            != std::string::npos
        );
    }

    TEST(U1Dialog, CancelledRequestIsNotSent) {
        atm::test::TestAtmContext f;
        f.input.str("2\nno\n");

        auto session = makeSession();

        const Nav result = supportDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Cancel);
        EXPECT_EQ(f.support.sendCount, 0);
        EXPECT_TRUE(f.support.lastMessage.empty());
    }

    TEST(U1Dialog, EofDoesNotSendRequest) {
        atm::test::TestAtmContext f;
        f.input.str("1\n");

        auto session = makeSession();

        const Nav result = supportDialog(f.ctx, session);

        EXPECT_EQ(result, Nav::Exit);
        EXPECT_EQ(f.support.sendCount, 0);
    }

    TEST(U1Dialog, SendFailureDoesNotReportSuccess) {
        atm::test::TestAtmContext f;
        FailingSupportChannel failingSupport;

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
            f.printer,
            failingSupport,
            f.clock
        );

        f.input.str("1\nyes\n");
        auto session = makeSession();

        bool caught = false;

        try {
            supportDialog(ctx, session);
        }
        catch (const std::runtime_error&) {
            caught = true;
        }

        EXPECT_TRUE(caught);
        EXPECT_EQ(failingSupport.attempts, 1);
        EXPECT_TRUE(
            f.output.str().find("Your support request has been sent.")
            == std::string::npos
        );
    }

} // namespace

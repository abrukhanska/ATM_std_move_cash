#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "application/account_service.hpp"
#include "application/auth_service.hpp"
#include "application/card_administration_service.hpp"
#include "application/cash_service.hpp"
#include "application/deposit_service.hpp"
#include "application/device_service.hpp"
#include "application/monitoring_service.hpp"
#include "application/payment_service.hpp"
#include "application/pin_hasher.hpp"
#include "application/transfer_service.hpp"
#include "application/withdrawal_service.hpp"

#include "domain/account.hpp"
#include "domain/atm_config.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/money.hpp"
#include "domain/types.hpp"

#include "infrastructure/sql/database.hpp"
#include "infrastructure/sql/data_seeder.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/atm_state.hpp"
#include "presentation/console_ui.hpp"
#include "presentation/session.hpp"
#include "presentation/dialogs.hpp"
#include "presentation/idle_state.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/support_channel.hpp"

#include <chrono>

int main()
{
    try {
        using namespace atm;

        // 1. Configuration
        const domain::AtmConfig config;

        // TODO: звірити тип Clock із domain/types.hpp
        domain::Clock clock = [] {
            return std::chrono::system_clock::now();
            };

        // 2. Database
        infrastructure::sql::Database storage("atm.db");

        // 3. PIN hasher
        application::PinHasher pinHasher;

        // 4. Demo cards
        const auto clientCard =
            domain::CardNumber::parse("4000000000000001");

        const auto secondClientCard =
            domain::CardNumber::parse("4000000000000002");

        const auto serviceCard =
            domain::CardNumber::parse("4000000000000003");

        const std::vector<domain::Card> cards{
            domain::Card(
                clientCard,
                "Alice",
                pinHasher.hash("1234"),
                domain::CardKind::Client
            ),
            domain::Card(
                secondClientCard,
                "Bob",
                pinHasher.hash("5678"),
                domain::CardKind::Client
            ),
            domain::Card(
                serviceCard,
                "Operator",
                pinHasher.hash("9999"),
                domain::CardKind::Service
            )
        };

        const std::vector<domain::Account> accounts{
            domain::Account(
                1, clientCard, domain::Money::uah(20000), true
            ),
            domain::Account(
                2, clientCard, domain::Money::uah(5000), false
            ),
            domain::Account(
                3, secondClientCard, domain::Money::uah(15000), true
            )
        };

        infrastructure::sql::seedDemoDataIfEmpty(
            storage, cards, accounts
        );

        infrastructure::sql::validateDatabase(storage);

        // 5. Application services
        application::AuthService auth(storage, pinHasher, clock);

        application::AccountService accountService(storage);

        application::WithdrawalService withdrawal(
            storage, config, clock
        );

        application::DepositService deposit(storage, clock);

        application::TransferService transfer(storage, clock);

        application::PaymentService payment(storage, clock);

        application::CashService cash(storage, clock);

        application::MonitoringService monitoring(
            storage, config
        );

        application::CardAdministrationService cardAdministration(
            storage, clock
        );

        application::DeviceService device(storage, clock);

        // 6. Presentation
        presentation::ConsoleUi ui(std::cin, std::cout);

        // TODO: звірити конструктор ConsoleReceiptPrinter
        presentation::ConsoleReceiptPrinter printer(std::cout);

        presentation::LoggingSupportChannel support("support.log");

        // 7. Context
        presentation::AtmContext ctx(
            auth,
            accountService,
            withdrawal,
            deposit,
            transfer,
            payment,
            cash,
            monitoring,
            cardAdministration,
            device,
            ui,
            printer,
            support,
            clock
        );

        // 8. Client menu
        ctx.clientMenu = {
            { "Balance", [&](presentation::Session& session) {
                return presentation::balanceDialog(ctx, session);
            }},
            { "Withdraw", [&](presentation::Session& session) {
                return presentation::withdrawDialog(ctx, session);
            }},
            { "Deposit", [&](presentation::Session& session) {
                return presentation::depositDialog(ctx, session);
            }},
            { "Transfer", [&](presentation::Session& session) {
                return presentation::transferDialog(ctx, session);
            }},
            { "Payments", [&](presentation::Session& session) {
                return presentation::paymentDialog(ctx, session);
            }},
            { "Change PIN", [&](presentation::Session& session) {
                return presentation::changePinDialog(ctx, session);
            }},
            { "Contact support", [&](presentation::Session& session) {
                return presentation::supportDialog(ctx, session);
            }}
        };

        // 9. Service menu
        ctx.serviceMenu = {
            { "Cash inventory", [&](presentation::Session& session) {
                return presentation::cashInventoryDialog(ctx, session);
            }},
            { "Load cash", [&](presentation::Session& session) {
                return presentation::cashLoadDialog(ctx, session);
            }},
            { "Unload cash", [&](presentation::Session& session) {
                return presentation::cashUnloadDialog(ctx, session);
            }},
            { "Recent operations", [&](presentation::Session& session) {
                return presentation::journalDialog(ctx, session);
            }},
            { "Statistics", [&](presentation::Session& session) {
                return presentation::statisticsDialog(ctx, session);
            }},
            { "Retained cards", [&](presentation::Session& session) {
                return presentation::retainedCardsDialog(ctx, session);
            }},
            { "Release card", [&](presentation::Session& session) {
                return presentation::releaseCardDialog(ctx, session);
            }},
            { "ATM mode", [&](presentation::Session& session) {
                return presentation::deviceModeDialog(ctx, session);
            }}
        };

        // 10. State machine
        std::unique_ptr<presentation::AtmState> state =
            std::make_unique<presentation::IdleState>();

        while (!ctx.exitRequested) {
            if (!state) {
                state = std::make_unique<presentation::IdleState>();
            }

            state = state->handle(ctx);
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ATM fatal error: " << e.what() << '\n';
        return 1;
    }
}

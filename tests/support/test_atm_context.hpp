#ifndef ATM_TEST_SUPPORT_TEST_ATM_CONTEXT_HPP
#define ATM_TEST_SUPPORT_TEST_ATM_CONTEXT_HPP

#include "support/transactional_test_storage.hpp"

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

#include "domain/atm_config.hpp"
#include "domain/types.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/console_ui.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/support_channel.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <string_view>

namespace atm::test {

    struct FakeSupportChannel : presentation::ISupportChannel {
        std::string lastMessage;
        int sendCount = 0;

        void send(std::string_view message) override {
            lastMessage = std::string(message);
            ++sendCount;
        }
    };

    struct TestAtmContext {
        domain::AtmConfig config;

        domain::Clock clock = [] {
            return std::chrono::system_clock::now();
            };

        // In-memory storage without SQLite.
        test_support::TestStorage storage;

        application::PinHasher pinHasher;

        application::AuthService auth;
        application::AccountService accounts;
        application::WithdrawalService withdrawal;
        application::DepositService deposit;
        application::TransferService transfer;
        application::PaymentService payment;
        application::CashService cash;
        application::MonitoringService monitoring;
        application::CardAdministrationService cardAdministration;
        application::DeviceService device;

        std::istringstream input;
        std::ostringstream output;

        presentation::ConsoleUi ui;
        presentation::ConsoleReceiptPrinter printer;
        FakeSupportChannel support;

        // Context must be initialized after its dependencies.
        presentation::AtmContext ctx;

        TestAtmContext()
            : auth(storage, pinHasher, clock),
            accounts(storage),
            withdrawal(storage, config, clock),
            deposit(storage, clock),
            transfer(storage, clock),
            payment(storage, clock),
            cash(storage, clock),
            monitoring(storage, config),
            cardAdministration(storage, clock),
            device(storage, clock),
            ui(input, output),
            printer(output),
            ctx(
                auth,
                accounts,
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
            )
        {
        }
    };

} // namespace atm::test

#endif // ATM_TEST_SUPPORT_TEST_ATM_CONTEXT_HPP

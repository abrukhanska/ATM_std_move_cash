#ifndef ATM_PRESENTATION_ATM_CONTEXT_HPP
#define ATM_PRESENTATION_ATM_CONTEXT_HPP

#include "presentation/menu_item.hpp"
#include "domain/types.hpp"

#include <vector>

namespace atm::application {
    class AuthService;
    class AccountService;
    class WithdrawalService;
    class DepositService;
    class TransferService;
    class PaymentService;
    class CashService;
    class MonitoringService;
    class CardAdministrationService;
    class DeviceService;
}

namespace atm::presentation {

    class IUserInterface;
    class IReceiptPrinter;
    class ISupportChannel;

    class AtmContext {
    public:
        application::AuthService& auth;
        application::AccountService& accounts;
        application::WithdrawalService& withdrawal;
        application::DepositService& deposit;
        application::TransferService& transfer;
        application::PaymentService& payment;
        application::CashService& cash;
        application::MonitoringService& monitoring;
        application::CardAdministrationService& cardAdministration;
        application::DeviceService& device;

        IUserInterface& ui;
        IReceiptPrinter& printer;
        ISupportChannel& support;

        std::vector<MenuItem> clientMenu;
        std::vector<MenuItem> serviceMenu;

        domain::Clock clock;
        bool exitRequested{ false };



        explicit AtmContext(
            application::AuthService& auth,
            application::AccountService& accounts,
            application::WithdrawalService& withdrawal,
            application::DepositService& deposit,
            application::TransferService& transfer,
            application::PaymentService& payment,
            application::CashService& cash,
            application::MonitoringService& monitoring,
            application::CardAdministrationService& cardAdministration,
            application::DeviceService& device,
            IUserInterface& ui,
            IReceiptPrinter& printer,
            ISupportChannel& support,
            domain::Clock clock
        );


        AtmContext(const AtmContext&) = delete;
        AtmContext& operator=(const AtmContext&) = delete;
        AtmContext(AtmContext&&) = delete;
        AtmContext& operator=(AtmContext&&) = delete;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_ATM_CONTEXT_HPP
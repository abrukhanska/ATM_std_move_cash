#include "presentation/atm_context.hpp"

#include <utility>

namespace atm::presentation {

    AtmContext::AtmContext(
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
    )
        : auth(auth),
        accounts(accounts),
        withdrawal(withdrawal),
        deposit(deposit),
        transfer(transfer),
        payment(payment),
        cash(cash),
        monitoring(monitoring),
        cardAdministration(cardAdministration),
        device(device),
        ui(ui),
        printer(printer),
        support(support),
        clock(std::move(clock))
    {
    }

} // namespace atm::presentation
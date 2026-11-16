#include "presentation/dialogs.hpp"

#include "presentation/atm_context.hpp"
#include "presentation/session.hpp"
#include "presentation/user_interface.hpp"
#include "presentation/receipt_printer.hpp"

#include "application/withdrawal_service.hpp"
#include "domain/domain_error.hpp"
#include "domain/money.hpp"
#include "domain/enums.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <limits>
#include <ios>

namespace atm::presentation {

    Nav withdrawDialog(AtmContext& ctx, Session& session)
    {
        if (!session.account.has_value()) {
            ctx.ui.showError("No account selected.");
            return Nav::Cancel;
        }

        const auto quickAmounts = ctx.withdrawal.quickAmounts();

        std::vector<std::string> options;

        for (const auto& amount : quickAmounts) {
            options.push_back(amount.format());
        }

        options.push_back("Enter custom amount");

        const Reply choice = ctx.ui.choose("Withdraw cash", options);

        if (choice.nav != Nav::Ok) {
            return choice.nav;
        }

        if (!choice.index.has_value() ||
            *choice.index >= options.size()) {
            ctx.ui.showError("Invalid selection.");
            return Nav::Cancel;
        }

        domain::Money amount;

        if (*choice.index < quickAmounts.size()) {
            amount = quickAmounts[*choice.index];
        }
        else {
            const Reply input = ctx.ui.prompt(
                "Enter amount in UAH (multiple of 100):"
            );

            if (input.nav != Nav::Ok) {
                return input.nav;
            }


            try {
                const std::string& text = input.text;
                const std::size_t separator = text.find_first_of(".,");

                if (text.empty() ||
                    (separator != std::string::npos &&
                        (separator == 0 ||
                            separator == text.size() - 1 ||
                            text.find_first_of(".,", separator + 1) != std::string::npos ||
                            text.size() - separator - 1 > 2))) {
                    throw std::invalid_argument("Invalid amount");
                }

                const std::size_t wholeEnd =
                    separator == std::string::npos ? text.size() : separator;

                constexpr std::int64_t maxValue =
                    (std::numeric_limits<std::int64_t>::max)();

                std::int64_t hryvnias = 0;

                for (std::size_t i = 0; i < wholeEnd; ++i) {
                    if (text[i] < '0' || text[i] > '9') {
                        throw std::invalid_argument("Invalid amount");
                    }

                    const int digit = text[i] - '0';


                    if (hryvnias > (maxValue / 1000) ||
                        (hryvnias == maxValue / 1000 &&
                            digit > (maxValue / 100) % 10)) 
                    {
                        throw std::out_of_range("Amount too large");
                    }

                    hryvnias = hryvnias * 10 + digit;
                }

                std::int64_t kopiykas = hryvnias * 100;

                if (separator != std::string::npos) {
                    const int first = text[separator + 1] - '0';

                    if (first < 0 || first > 9) {
                        throw std::invalid_argument("Invalid amount");
                    }

                    int fraction = first * 10;

                    if (separator + 2 < text.size()) {
                        const int second = text[separator + 2] - '0';

                        if (second < 0 || second > 9) {
                            throw std::invalid_argument("Invalid amount");
                        }

                        fraction += second;
                    }

                    if (kopiykas > maxValue - fraction) {
                        throw std::out_of_range("Amount too large");
                    }

                    kopiykas += fraction;
                }

                if (kopiykas == 0 || kopiykas % 10'000 != 0) {
                    ctx.ui.showError(
                        "Amount must be positive and a multiple of 100 UAH."
                    );
                    return Nav::Cancel;
                }

                amount = domain::Money::ofKopiykas(kopiykas);
            }

            catch (const std::invalid_argument&) {
                ctx.ui.showError("Invalid amount.");
                return Nav::Cancel;
            }
            catch (const std::out_of_range&) {
                ctx.ui.showError("Amount is too large.");
                return Nav::Cancel;
            }
            catch (const domain::DomainError& e) {
                if (e.reason() == domain::ErrorReason::InvalidAmount ||
                    e.reason() == domain::ErrorReason::Overflow) {
                    ctx.ui.showError("Invalid amount.");
                    return Nav::Cancel;
                }

                throw;
            }
        }

        const Nav confirmation = ctx.ui.confirm(
            "Withdraw " + amount.format() + "?"
        );

        if (confirmation != Nav::Ok) {
            return confirmation;
        }

        std::optional<domain::JournalEntry> entry;

        try {
            entry = ctx.withdrawal.withdraw(
                session.card,
                *session.account,
                amount
            );
        }
        catch (const domain::DomainError& e) {
            switch (e.reason()) {
            case domain::ErrorReason::InsufficientFunds:
                ctx.ui.showError("Insufficient funds.");
                return Nav::Cancel;

            case domain::ErrorReason::DailyLimitExceeded:
                ctx.ui.showError("Daily withdrawal limit exceeded.");
                return Nav::Cancel;

            case domain::ErrorReason::AtmOutOfCash:
                ctx.ui.showError("ATM has insufficient cash.");
                return Nav::Cancel;

            case domain::ErrorReason::InvalidAmount:
                ctx.ui.showError("Invalid withdrawal amount.");
                return Nav::Cancel;

            default:
                throw;
            }
        }


        std::vector<std::string> details{
            "Amount: " + amount.format() + " UAH",
            "Dispensed banknotes:"
        };

        if (entry.has_value()) {
            for (const auto denomination : domain::kAllDenominations) {
                const auto count = entry->cash.count(denomination);

                if (count > 0) {
                    details.push_back(
                        std::to_string(domain::denominationUah(denomination))
                        + " UAH x " + std::to_string(count)
                    );
                }
            }
        }

        ctx.ui.showScreen("Withdrawal successful", details);


        const Nav receiptChoice = ctx.ui.confirm("Print receipt?");


        if (receiptChoice == Nav::Ok) {
            try {
                ctx.printer.print(*entry);
            }
            catch (const std::ios_base::failure&) {
                try {
                    ctx.ui.showError(
                        "Withdrawal completed, but receipt printing failed."
                    );
                }
                catch (const std::ios_base::failure&) {
                    // The output stream may also be unavailable.
                }

                return Nav::Exit;
            }
        }


        return receiptChoice == Nav::Exit
            ? Nav::Exit
            : Nav::Ok;
    }

} // namespace atm::presentation

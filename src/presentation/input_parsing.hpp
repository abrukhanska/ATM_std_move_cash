#ifndef ATM_PRESENTATION_INPUT_PARSING_HPP
#define ATM_PRESENTATION_INPUT_PARSING_HPP

#include "domain/money.hpp"
#include "domain/card_number.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>


#include "presentation/atm_context.hpp"
#include "presentation/user_interface.hpp"

#include "domain/banknote_set.hpp"
#include "domain/enums.hpp"

#include <string>


namespace atm::presentation {


    [[nodiscard]] inline domain::CardNumber parseCardNumber(std::string_view text)
    {
        constexpr std::string_view whitespace = " \t\n\r\f\v";

        const auto first = text.find_first_not_of(whitespace);

        if (first == std::string_view::npos) {
            throw std::invalid_argument("Invalid card number");
        }

        const auto last = text.find_last_not_of(whitespace);
        text = text.substr(first, last - first + 1);

        return domain::CardNumber::parse(text);
    }


    [[nodiscard]] inline Nav readBanknotes(
        AtmContext& ctx,
        domain::BanknoteSet& notes)
    {
        for (std::size_t i = 0; i < domain::kAllDenominations.size(); ++i) {
            const std::string denomination =
                std::to_string(
                    domain::denominationUah(domain::kAllDenominations[i])
                ) + " UAH";

            while (true) {
                const Reply input = ctx.ui.prompt(
                    "Number of " + denomination +
                    " banknotes (0 to skip):"
                );

                if (input.nav != Nav::Ok) {
                    return input.nav;
                }

                if (input.text.empty() ||
                    input.text.find_first_not_of("0123456789") != std::string::npos) {
                    ctx.ui.showError("Invalid banknote count.");
                    continue;
                }

                try {
                    std::size_t pos = 0;
                    const auto count = std::stoull(input.text, &pos);

                    if (pos != input.text.size() ||
                        count > std::numeric_limits<std::uint32_t>::max()) {
                        ctx.ui.showError("Invalid banknote count.");
                        continue;
                    }

                    notes = notes.with(
                        domain::kAllDenominations[i],
                        static_cast<std::uint32_t>(count)
                    );

                    break;
                }
                catch (const std::invalid_argument&) {
                    ctx.ui.showError("Invalid banknote count.");
                }
                catch (const std::out_of_range&) {
                    ctx.ui.showError("Banknote count is too large.");
                }
            }
        }

        return Nav::Ok;
    }


    [[nodiscard]] inline domain::Money parseMoney(std::string_view text)
    {
        constexpr std::string_view whitespace = " \t\n\r\f\v";

        const auto first = text.find_first_not_of(whitespace);
        if (first == std::string_view::npos) {
            throw std::invalid_argument("Invalid amount");
        }

        const auto last = text.find_last_not_of(whitespace);
        text = text.substr(first, last - first + 1);

        const auto separator = text.find_first_of(".,");

        if (separator == 0 ||
            (separator != std::string_view::npos &&
                (separator == text.size() - 1 ||
                    text.find_first_of(".,", separator + 1) != std::string_view::npos ||
                    text.size() - separator - 1 > 2))) {
            throw std::invalid_argument("Invalid amount");
        }

        const auto wholeEnd =
            separator == std::string_view::npos ? text.size() : separator;

        constexpr std::int64_t maxValue =
            (std::numeric_limits<std::int64_t>::max)();

        std::int64_t hryvnias = 0;

        for (std::size_t i = 0; i < wholeEnd; ++i) {
            if (text[i] < '0' || text[i] > '9') {
                throw std::invalid_argument("Invalid amount");
            }

            const int digit = text[i] - '0';

            if (hryvnias > (maxValue / 100 - digit) / 10) {
                throw std::out_of_range("Amount too large");
            }

            hryvnias = hryvnias * 10 + digit;
        }

        std::int64_t kopiykas = hryvnias * 100;

        if (separator != std::string_view::npos) {
            const int firstDigit = text[separator + 1] - '0';

            if (firstDigit < 0 || firstDigit > 9) {
                throw std::invalid_argument("Invalid amount");
            }

            int fraction = firstDigit * 10;

            if (separator + 2 < text.size()) {
                const int secondDigit = text[separator + 2] - '0';

                if (secondDigit < 0 || secondDigit > 9) {
                    throw std::invalid_argument("Invalid amount");
                }

                fraction += secondDigit;
            }

            if (kopiykas > maxValue - fraction) {
                throw std::out_of_range("Amount too large");
            }

            kopiykas += fraction;
        }

        if (kopiykas == 0) {
            throw std::invalid_argument("Invalid amount");
        }

        return domain::Money::ofKopiykas(kopiykas);
    }

} // namespace atm::presentation

#endif // ATM_PRESENTATION_INPUT_PARSING_HPP

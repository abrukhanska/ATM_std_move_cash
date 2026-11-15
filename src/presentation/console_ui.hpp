#ifndef ATM_PRESENTATION_CONSOLE_UI_HPP
#define ATM_PRESENTATION_CONSOLE_UI_HPP

#include "presentation/user_interface.hpp"

#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace atm::presentation {

    class ConsoleUi : public IUserInterface {
    private:
        std::istream& in_;
        std::ostream& out_;

    public:
        ConsoleUi(std::istream& in, std::ostream& out);

        void showScreen(
            const std::string& title,
            const std::vector<std::string>& lines
        ) override;

        void showError(const std::string& message) override;

        Reply prompt(const std::string& message) override;

        Reply promptSecret(const std::string& message) override;

        Reply choose(
            const std::string& title,
            const std::vector<std::string>& options
        ) override;

        Nav confirm(const std::string& message) override;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_CONSOLE_UI_HPP

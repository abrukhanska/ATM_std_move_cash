#ifndef ATM_PRESENTATION_USER_INTERFACE_HPP
#define ATM_PRESENTATION_USER_INTERFACE_HPP

#include "presentation/ui_types.hpp"

#include <string>
#include <vector>

namespace atm::presentation {

    class IUserInterface {
    public:
        virtual ~IUserInterface() noexcept = default;

        virtual void showScreen(
            const std::string& title,
            const std::vector<std::string>& lines
        ) = 0;

        virtual void showError(const std::string& message) = 0;

        virtual Reply prompt(const std::string& message) = 0;

        virtual Reply promptSecret(const std::string& message) = 0;

        virtual Reply choose(
            const std::string& title,
            const std::vector<std::string>& options
        ) = 0;

        virtual Nav confirm(const std::string& message) = 0;
    };

}  // namespace atm::presentation

#endif // ATM_PRESENTATION_USER_INTERFACE_HPP

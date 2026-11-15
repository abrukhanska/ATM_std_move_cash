#ifndef ATM_PRESENTATION_MENU_ITEM_HPP
#define ATM_PRESENTATION_MENU_ITEM_HPP

#include "presentation/ui_types.hpp"

#include <functional>
#include <string>

namespace atm::presentation {

    class Session;

    struct MenuItem {
        std::string title;
        std::function<Nav(Session&)> run;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_MENU_ITEM_HPP
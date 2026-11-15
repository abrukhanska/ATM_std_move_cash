#ifndef ATM_PRESENTATION_UI_TYPES_HPP
#define ATM_PRESENTATION_UI_TYPES_HPP

#include <cstddef>
#include <optional>
#include <string>

namespace atm::presentation {

    enum class Nav {
        Ok,
        Cancel,
        Exit
    };

    struct Reply {
        Nav nav;
        std::string text;
        std::optional<std::size_t> index;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_UI_TYPES_HPP

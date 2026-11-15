#ifndef ATM_PRESENTATION_SESSION_HPP
#define ATM_PRESENTATION_SESSION_HPP

#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "domain/types.hpp"

#include <optional>
#include <utility>

namespace atm::presentation {

    struct Session {
        atm::domain::CardNumber card;
        atm::domain::CardKind kind;
        std::optional<atm::domain::AccountId> account;

        Session(atm::domain::CardNumber card,
            atm::domain::CardKind kind,
            std::optional<atm::domain::AccountId> account = std::nullopt)
            : card(std::move(card)), kind(kind), account(account) {
        }

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        Session(Session&&) noexcept = default;
        Session& operator=(Session&&) noexcept = default;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_SESSION_HPP

#include "application/auth_service.hpp"
#include "domain/transaction.hpp"
#include "application/detail/access.hpp"

namespace atm::application {

    CardInspection AuthService::inspect(const domain::CardNumber &actor) const
    {
        const auto card = storage_.cards().find(actor);
        if (!card.has_value()) return CardInspection::Unknown;
        if (card->status() == domain::CardStatus::Retained) return CardInspection::Retained;
        if (card->kind() == domain::CardKind::Client) return CardInspection::Client;
        return CardInspection::Service;
    }

    AuthStatus AuthService::authenticate(const domain::CardNumber &actor, const domain::PinCode &pin)
    {
        auto card = storage_.cards().find(actor);
        if (!card.has_value()) throw domain::DomainError(domain::ErrorReason::UnknownCard);
        if (card->status() == domain::CardStatus::Retained) return AuthStatus::Retained;

        std::string_view pinSV(pin.digits().data(), pin.digits().size());

        // Service Card
        if (card->kind() == domain::CardKind::Service) {
            if (pinHasher_.matches(pinSV, card->pinHash())) {
                return AuthStatus::Ok;
            }
            return AuthStatus::WrongPin;
        }

        domain::Transaction transaction(storage_);

        // Client Card
        const bool match = pinHasher_.matches(pinSV, card->pinHash());
        if (match) {
            card->registerSuccess();
            storage_.cards().save(*card);
            transaction.commit();
            return AuthStatus::Ok;
        }

        card->registerFailedAttempt();
        const bool retained = (card->status() == domain::CardStatus::Retained);
        storage_.cards().save(*card);

        if (retained) {
            const auto now = clock_();
            domain::JournalEntry entry;
            entry.atUtc = domain::toEpochSeconds(now);
            entry.localDay = domain::localDay(now);
            entry.kind = domain::EntryKind::CardRetained;
            entry.actor = actor;
            entry.subjectCard = actor;
            entry.amount = domain::Money::ofKopiykas(0);

            (void)storage_.journal().append(entry);
        }

        transaction.commit();
        return retained ? AuthStatus::Retained : AuthStatus::WrongPin;
    }

    void AuthService::changePin(const domain::CardNumber& actor, const domain::PinCode& oldPin, const domain::PinCode& newPin)
    {
        domain::Transaction transaction(storage_);

        auto card = detail::requireActiveClient(storage_, actor);

        const std::string_view oldPinSV(oldPin.digits().data(), oldPin.digits().size());
        const std::string_view newPinSV(newPin.digits().data(), newPin.digits().size());

        if (!pinHasher_.matches(oldPinSV, card.pinHash())) {
            throw domain::DomainError(domain::ErrorReason::IncorrectPin);
        }
        if (oldPin == newPin) {
            throw domain::DomainError(domain::ErrorReason::SamePin);
        }

        card.changePinHash(pinHasher_.hash(newPinSV));

        const auto now = clock_();
        domain::JournalEntry entry;
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = domain::EntryKind::PinChange;
        entry.actor = actor;
        entry.amount = domain::Money::ofKopiykas(0);

        storage_.cards().save(card);
        (void)storage_.journal().append(entry);
        transaction.commit();
    }

} // namespace atm::application

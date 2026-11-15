#include "application/auth_service.hpp"
#include "domain/transaction.hpp"

namespace atm::application {

    domain::CardInspection atm::application::AuthService::inspect(const domain::CardNumber &actor) const
    {
        const auto card = storage_.cards().find(actor);
        if (!card.has_value()) return domain::CardInspection::Unknown;
        if (card->status() == domain::CardStatus::Retained) return domain::CardInspection::Retained;
        if (card->kind() == domain::CardKind::Client) return domain::CardInspection::Client;
        return domain::CardInspection::Service;
    }

    domain::AuthStatus atm::application::AuthService::authenticate(const domain::CardNumber &actor, const domain::PinCode &pin)
    {
        auto card = storage_.cards().find(actor);
        if (!card.has_value()) throw domain::DomainError(domain::ErrorReason::UnknownCard);
        if (card->status() == domain::CardStatus::Retained) return domain::AuthStatus::Retained;

        std::string_view pinSV(pin.digits().data(), pin.digits().size());

        // Service Card
        if (card->kind() == domain::CardKind::Service) {
            if (pinHasher_.matches(pinSV, card->pinHash())) {
                return domain::AuthStatus::Ok;
            }
            return domain::AuthStatus::WrongPin;
        }

        // Client Card
        const bool match = pinHasher_.matches(pinSV, card->pinHash());
        if (match) {
            card->registerSuccess();
            domain::Transaction transaction(storage_);
            storage_.cards().save(*card);
            transaction.commit();
            return domain::AuthStatus::Ok;
        } else {
            card->registerFailedAttempt();
            const bool retained = (card->status() == domain::CardStatus::Retained);

            domain::Transaction transaction(storage_);
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

                storage_.journal().append(entry);
            }

            transaction.commit();

            return retained ? domain::AuthStatus::Retained : domain::AuthStatus::WrongPin;
        }
    }

    void atm::application::AuthService::changePin(const domain::CardNumber &actor, const domain::PinCode &oldPin, const domain::PinCode &newPin)
    {
        auto card = storage_.cards().find(actor);
        if (!card.has_value()) throw domain::DomainError(domain::ErrorReason::UnknownCard);
        if (card->kind() != domain::CardKind::Client || card->status() != domain::CardStatus::Active) {
            throw domain::DomainError(domain::ErrorReason::AccessDenied);
        }

        std::string_view oldPinSV(oldPin.digits().data(), oldPin.digits().size());
        std::string_view newPinSV(newPin.digits().data(), newPin.digits().size());

        if (!pinHasher_.matches(oldPinSV, card->pinHash())) {
            throw domain::DomainError(domain::ErrorReason::IncorrectPin);
        }

        if (oldPin == newPin) throw domain::DomainError(domain::ErrorReason::SamePin);

        const std::string newHash = pinHasher_.hash(newPinSV);
        card->changePinHash(newHash);

        const auto now = clock_();
        domain::JournalEntry entry;
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = domain::EntryKind::PinChange;
        entry.actor = actor;
        entry.amount = domain::Money::ofKopiykas(0);

        domain::Transaction transaction(storage_);
        storage_.cards().save(*card);
        storage_.journal().append(entry);
        transaction.commit();
    }

} // namespace atm::application

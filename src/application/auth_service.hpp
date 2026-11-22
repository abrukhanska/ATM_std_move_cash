#ifndef ATM_APPLICATION_AUTH_SERVICE_HPP
#define ATM_APPLICATION_AUTH_SERVICE_HPP

#include "domain/istorage.hpp"
#include "application/pin_hasher.hpp"
#include "application/app_types.hpp"
#include "domain/atm_config.hpp"
#include "domain/pin_code.hpp"

namespace atm::application {

    class AuthService {
    public:
        AuthService(domain::IStorage& storage, const PinHasher& pinHasher, domain::Clock clock)
            : storage_(storage), pinHasher_(pinHasher), clock_(std::move(clock)) {
                if(!clock_) throw domain::DomainError(domain::ErrorReason::InvalidData);
            }

        [[nodiscard]] CardInspection inspect(const domain::CardNumber& actor) const;
        AuthStatus authenticate(const domain::CardNumber& actor, const domain::PinCode& pin);
        void changePin(const domain::CardNumber& actor, const domain::PinCode& oldPin, const domain::PinCode& newPin);

    private:
        domain::IStorage& storage_;
        const PinHasher& pinHasher_;
        domain::Clock clock_;
    };

} // namespace atm::application

#endif // ATM_APPLICATION_AUTH_SERVICE_HPP

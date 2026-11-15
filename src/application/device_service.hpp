#ifndef ATM_APPLICATION_DEVICE_SERVICE_HPP
#define ATM_APPLICATION_DEVICE_SERVICE_HPP

#include <domain/istorage.hpp>

namespace atm::application {

    class DeviceService {
    public:
        DeviceService(domain::IStorage& storage, domain::Clock clock)
            : storage_(storage), clock_(std::move(clock)) {}

        [[nodiscard]] bool isInService() const noexcept { return inService_; }
        [[nodiscard]] bool isBroken() const noexcept { return storage_.broken(); }

        domain::JournalEntry setInService(const domain::CardNumber actor, bool enable);

    private:
        domain::IStorage& storage_;
        domain::Clock clock_;
        bool inService_{true};
    };

} // namespace atm::application

#endif // ATM_APPLICATION_DEVICE_SERVICE_HPP

#ifndef ATM_PRESENTATION_SUPPORT_CHANNEL_HPP
#define ATM_PRESENTATION_SUPPORT_CHANNEL_HPP

#include <fstream>
#include <string>
#include <string_view>

namespace atm::presentation {

    class ISupportChannel {
    public:
        virtual ~ISupportChannel() noexcept = default;

        virtual void send(std::string_view message) = 0;
    };

    class LoggingSupportChannel : public ISupportChannel {
    private:
        std::ofstream log_;

    public:
        explicit LoggingSupportChannel(const std::string& filename);

        void send(std::string_view message) override;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_SUPPORT_CHANNEL_HPP

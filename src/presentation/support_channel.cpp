#include "presentation/support_channel.hpp"

#include <stdexcept>

namespace atm::presentation {

    LoggingSupportChannel::LoggingSupportChannel(const std::string& filename)
        : log_(filename, std::ios::app) {

        if (!log_.is_open()) {
            throw std::runtime_error("Failed to open support log file.");
        }
    }

    void LoggingSupportChannel::send(std::string_view message) {
        log_ << message << '\n';

        if (!log_) {
            throw std::runtime_error("Failed to write support message.");
        }
    }

} // namespace atm::presentation
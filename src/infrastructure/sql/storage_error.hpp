#ifndef ATM_INFRASTRUCTURE_STORAGE_ERROR_HPP
#define ATM_INFRASTRUCTURE_STORAGE_ERROR_HPP

#include <stdexcept>

namespace atm::infrastructure::sql {

    class StorageError : public std::runtime_error {
    public:
        StorageError(const std::string& operation, int sql, const std::string& reason)
            : std::runtime_error(message(operation, sql, reason)), operation_(operation), sql_(sql), reason_(reason) {}

        [[nodiscard]] const std::string& operation() const noexcept { return operation_; }
        [[nodiscard]] int sqlCode() const noexcept { return sql_; }
        [[nodiscard]] const std::string& reason() const noexcept { return reason_; }

    private:
        std::string operation_;
        int sql_{0};
        std::string reason_;
    
        static std::string message (const std::string& operation, int sql, const std::string& reason) {
            return "StorageError: [operation = " + operation + "], [sql code = " + std::to_string(sql) + "], [reason = " + reason + "]";
        }
    };
    
} // namespace atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_STORAGE_ERROR_HPP
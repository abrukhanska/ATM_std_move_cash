#ifndef ATM_DOMAIN_TRANSACTION_HPP
#define ATM_DOMAIN_TRANSACTION_HPP

#include "istorage.hpp"

namespace atm::domain {

    class Transaction {
    public:
        explicit Transaction(IStorage& storage) : storage_(storage) {
             storage_.begin();
        }

        ~Transaction() noexcept {
            if (active_) storage_.rollback();
        }

        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction(Transaction&&) = delete;
        Transaction& operator=(Transaction&&) = delete;

        void commit() {
            if (active_) {
                storage_.commit();
                active_ = false;
            }
        }

    private:
        IStorage& storage_;
        bool active_{true};
};

} // namespace atm::domain

#endif // ATM_DOMAIN_TRANSACTION_HPP
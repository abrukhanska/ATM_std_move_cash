#ifndef ATM_INFRASTRUCTURE_IN_MEMORY_CASH_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_IN_MEMORY_CASH_REPOSITORY_HPP

#include "domain/repositories.hpp"

namespace atm::infrastructure {

class InMemoryCashRepository : public domain::ICashRepository {
public:
    [[nodiscard]] domain::BanknoteSet load() const override {
        return cash_;
    }

    void save(const domain::BanknoteSet& banknotes) override {
        cash_ = banknotes;
    }

private:
    domain::BanknoteSet cash_{};
};

} // namespace atm::infrastructure

#endif // ATM_INFRASTRUCTURE_IN_MEMORY_CASH_REPOSITORY_HPP
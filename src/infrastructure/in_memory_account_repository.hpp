#ifndef ATM_INFRASTRUCTURE_IN_MEMORY_ACCOUNT_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_IN_MEMORY_ACCOUNT_REPOSITORY_HPP

#include "domain/repositories.hpp"
#include <algorithm>
#include <vector>

namespace atm::infrastructure {

class InMemoryAccountRepository : public domain::IAccountRepository {
public:
    [[nodiscard]] std::optional<domain::Account> find(domain::AccountId id) const override {
        for (const auto& acc : accounts_) {
            if (acc.id() == id) {
                return acc;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<domain::Account> findPrimary(const domain::CardNumber& cardNumber) const override {
        for (const auto& acc : accounts_) {
            if (acc.owner() == cardNumber && acc.isPrimary()) {
                return acc;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<domain::Account> listByCard(const domain::CardNumber& cardNumber) const override {
        std::vector<domain::Account> result;
        for (const auto& acc : accounts_) {
            if (acc.owner() == cardNumber) {
                result.push_back(acc);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.id() < b.id();
        });
        return result;
    }

    void save(const domain::Account& account) override {
        for (auto& acc : accounts_) {
            if (acc.id() == account.id()) {
                acc = account;
                return;
            }
        }
        accounts_.push_back(account);
    }

private:
    std::vector<domain::Account> accounts_;
};

} // namespace atm::infrastructure

#endif // ATM_INFRASTRUCTURE_IN_MEMORY_ACCOUNT_REPOSITORY_HPP
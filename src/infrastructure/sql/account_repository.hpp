#ifndef ATM_INFRASTRUCTURE_SQL_ACCOUNT_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_SQL_ACCOUNT_REPOSITORY_HPP

#include "domain/iaccount_repository.hpp"

namespace atm::infrastructure::sql {

	class Database;

	class AccountRepository : public domain::IAccountRepository {
	public:
		explicit AccountRepository(Database& db) : db_(db) {}

		[[nodiscard]] std::optional<domain::Account> find(domain::AccountId id) const override;
		[[nodiscard]] std::optional<domain::Account> findPrimary(const domain::CardNumber& number) const override;
		[[nodiscard]] std::vector<domain::Account> listByCard(const domain::CardNumber& number) const override;
		void save(const domain::Account& account) override;
		
	private:
		Database& db_;
	};

} // atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_SQL_ACCOUNT_REPOSITORY_HPP

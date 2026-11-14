#ifndef ATM_INFRASTRUCTURE_SQL_JOURNAL_REPOSITORY_HPP
#define ATM_INFRASTRUCTURE_SQL_JOURNAL_REPOSITORY_HPP

#include "domain/ijournal_repository.hpp"

namespace atm::infrastructure::sql {

	class Database;

	class JournalRepository : public domain::IJournalRepository {
	public:
		explicit JournalRepository(Database& db) : db_(db) {}

		domain::JournalEntry append(const domain::JournalEntry& entry) override;
		[[nodiscard]] virtual std::vector<domain::JournalEntry> last(std::size_t count) const override;
		[[nodiscard]] virtual domain::Money withdrawnOn(const domain::CardNumber& number, const std::string& day) const override;
		[[nodiscard]] virtual domain::Money sumAmount(domain::EntryKind kind) const override;
		
	private:
		Database& db_;
	};

} // atm::infrastructure::sql

#endif // ATM_INFRASTRUCTURE_SQL_JOURNAL_REPOSITORY_HPP

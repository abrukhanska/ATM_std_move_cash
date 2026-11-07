#include "application/withdrawal_service.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "application/detail/access.hpp"
#include "application/plan_dispense.hpp"
#include "domain/account.hpp"
#include "domain/banknote_set.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/istorage.hpp"
#include "domain/transaction.hpp"

namespace atm::application {

    using domain::DomainError;
    using domain::ErrorReason;

    namespace {

        constexpr std::int64_t kKopiykasIn100Uah = 10'000;

        [[nodiscard]] bool isValidWithdrawalAmount(domain::Money amount) noexcept {
            const auto kopiykas = amount.kopiykas();
            return kopiykas > 0 && kopiykas % kKopiykasIn100Uah == 0;
        }

        static_assert(std::is_nothrow_move_constructible_v<domain::JournalEntry>,
            "Returning a withdrawal result must not throw after commit");

    }  // namespace

    WithdrawalService::WithdrawalService(
        domain::IStorage& storage, const domain::AtmConfig& config, domain::Clock clock
    )
        : storage_(storage), config_(config), clock_(std::move(clock)) {
        if (!clock_ || config_.dailyWithdrawalLimit.kopiykas() == 0) {
            throw DomainError(ErrorReason::InvalidData);
        }
        for (const auto& amount : config_.quickAmounts) {
            if (!isValidWithdrawalAmount(amount)) {
                throw DomainError(ErrorReason::InvalidData);
            }
        }
    }

    domain::JournalEntry WithdrawalService::withdraw(
        const domain::CardNumber& actor, domain::AccountId from, domain::Money amount
    ) {
        if (!isValidWithdrawalAmount(amount)) {
            throw DomainError(ErrorReason::InvalidAmount);
        }

        domain::Transaction tx(storage_);
        (void)detail::requireActiveClient(storage_, actor);
        auto account = detail::requireOwnedAccount(storage_, actor, from);

        if (account.balance() < amount) {
            throw DomainError(ErrorReason::InsufficientFunds);
        }

        const auto now = clock_();
        std::string day = domain::localDay(now);
        auto& journal = storage_.journal();
        const auto withdrawnToday = journal.withdrawnOn(actor, day);
        const auto limit = config_.dailyWithdrawalLimit;

        // Short-circuit before subtraction; never add two potentially large amounts.
        if (withdrawnToday >= limit || amount > limit - withdrawnToday) {
            throw DomainError(ErrorReason::DailyLimitExceeded);
        }

        auto& cash = storage_.cash();
        const auto available = cash.load();
        const auto plan = planDispense(amount, available);
        const auto remaining = available.minus(plan);
        account.debit(amount);

        domain::JournalEntry entry{};
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = std::move(day);
        entry.kind = domain::EntryKind::Withdrawal;
        entry.actor = actor;
        entry.account = from;
        entry.amount = amount;
        entry.cash = plan;

        storage_.accounts().save(account);
        cash.save(remaining);
        auto saved = journal.append(entry);

        tx.commit();
        return saved;
    }

    std::vector<domain::Money> WithdrawalService::quickAmounts() const {
        return config_.quickAmounts;
    }

}  // namespace atm::application

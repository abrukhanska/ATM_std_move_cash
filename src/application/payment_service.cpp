#include "application/payment_service.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "application/detail/access.hpp"
#include "domain/account.hpp"
#include "domain/domain_error.hpp"
#include "domain/istorage.hpp"
#include "domain/transaction.hpp"

namespace atm::application {

    using domain::DomainError;
    using domain::ErrorReason;
    using domain::Payee;
    using domain::PayeeCategory;

    namespace {

        constexpr std::size_t kMobileNumberLength = 10;

        static_assert(std::is_nothrow_move_constructible_v<domain::JournalEntry>,
            "Returning a payment result must not throw after commit");

        [[nodiscard]] constexpr bool isValidCategory(PayeeCategory category) noexcept {
            return category == PayeeCategory::MobileOperator || category == PayeeCategory::Charity;
        }

        [[nodiscard]] bool isValidRequisite(PayeeCategory category, std::string_view requisite) {
            // A ported number keeps its prefix, so validate format only.
            switch (category) {
            case PayeeCategory::MobileOperator:
                return requisite.size() == kMobileNumberLength &&
                    std::all_of(requisite.begin(), requisite.end(), [](char c) {
                    return c >= '0' && c <= '9';
                        });
            case PayeeCategory::Charity:
                return requisite.empty();
            }
            return false;
        }

        std::vector<Payee> defaultPayees() {
            return {
                Payee{ 1, "Vodafone Ukraine", PayeeCategory::MobileOperator },
                Payee{ 3, "Kyivstar", PayeeCategory::MobileOperator },
                Payee{ 4, "lifecell", PayeeCategory::MobileOperator },
                Payee{ 2, "Come Back Alive", PayeeCategory::Charity },
                Payee{ 5, "UNITED24", PayeeCategory::Charity }
            };
        }

        void validateDirectory(const std::vector<Payee>& payees) {
            for (std::size_t i = 0; i < payees.size(); ++i) {
                if (payees[i].id <= 0 || payees[i].name.empty() || !isValidCategory(payees[i].category)) {
                    throw DomainError(ErrorReason::InvalidData);
                }
                for (std::size_t j = i + 1; j < payees.size(); ++j) {
                    if (payees[i].id == payees[j].id) {
                        throw DomainError(ErrorReason::InvalidData);
                    }
                }
            }
        }

    }  // namespace

    PaymentService::PaymentService(domain::IStorage& storage, domain::Clock clock)
        : PaymentService(storage, std::move(clock), defaultPayees()) {
    }

    PaymentService::PaymentService(
        domain::IStorage& storage, domain::Clock clock, std::vector<Payee> payees
    )
        : storage_(storage), clock_(std::move(clock)), payees_(std::move(payees)) {
        if (!clock_) {
            throw DomainError(ErrorReason::InvalidData);
        }
        validateDirectory(payees_);
    }

    std::vector<Payee> PaymentService::payees(PayeeCategory category) const {
        std::vector<Payee> result;
        for (const Payee& payee : payees_) {
            if (payee.category == category) {
                result.push_back(payee);
            }
        }
        return result;
    }

    domain::JournalEntry PaymentService::pay(
        const domain::CardNumber& actor, domain::AccountId from,
        std::int64_t payeeId, std::string_view requisite, domain::Money amount
    ) {
        const auto payee = std::find_if(payees_.begin(), payees_.end(), [payeeId](const Payee& candidate) {
            return candidate.id == payeeId;
            });
        if (payee == payees_.end()) {
            throw DomainError(ErrorReason::UnknownPayee);
        }
        if (!isValidRequisite(payee->category, requisite)) {
            throw DomainError(ErrorReason::InvalidRequisite);
        }
        if (amount.kopiykas() == 0) {
            throw DomainError(ErrorReason::InvalidAmount);
        }

        domain::Transaction tx(storage_);
        (void)detail::requireActiveClient(storage_, actor);
        auto account = detail::requireOwnedAccount(storage_, actor, from);
        account.debit(amount);

        const auto now = clock_();
        domain::JournalEntry entry{};
        entry.atUtc = domain::toEpochSeconds(now);
        entry.localDay = domain::localDay(now);
        entry.kind = domain::EntryKind::Payment;
        entry.actor = actor;
        entry.account = from;
        entry.amount = amount;
        entry.payeeId = payee->id;
        entry.requisite = requisite;

        storage_.accounts().save(account);
        auto saved = storage_.journal().append(entry);

        tx.commit();
        return saved;
    }

}  // namespace atm::application

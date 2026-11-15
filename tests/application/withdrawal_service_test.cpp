#include "application/withdrawal_service.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "application/detail/access.hpp"
#include "domain/domain_error.hpp"
#include "domain/istorage.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using namespace atm::domain;
    using atm::application::WithdrawalService;

    static_assert(!std::is_constructible_v<WithdrawalService, IStorage&, AtmConfig, Clock>);

    static_assert(std::is_same_v<
        decltype(std::declval<const WithdrawalService&>().quickAmounts()),
        std::vector<Money>
    >);

    using atm::test_support::TestStorage;
    using atm::test_support::InjectedStorageFailure;

    TimePoint localBeforeMidnight() {
        std::tm date{};
        date.tm_year = 2026 - 1900;
        date.tm_mon = 5;
        date.tm_mday = 15;
        date.tm_hour = 23;
        date.tm_min = 59;
        date.tm_sec = 59;
        date.tm_isdst = -1;
        const auto time = std::mktime(&date);
        if (time == static_cast<std::time_t>(-1)) {
            throw std::runtime_error("Could not construct a local test timestamp");
        }
        return std::chrono::system_clock::from_time_t(time);
    }

    struct Fixture {
        TestStorage storage;
        const AtmConfig config{};
        const CardNumber client = CardNumber::parse("4000000000000001");
        const CardNumber other = CardNumber::parse("4000000000000002");
        TimePoint now = localBeforeMidnight();
        int clockCalls = 0;

        WithdrawalService service{ storage, config, [this] {
            ++clockCalls;
            return now;
        } };

        Fixture() {
            storage.seed(Card(client, "Client", "hash", CardKind::Client));
            storage.seed(Card(other, "Other client", "hash", CardKind::Client));
            storage.seed(Account(1, client, Money::uah(30'000), true));
            storage.seed(Account(2, client, Money::uah(30'000)));
            storage.seed(Account(3, other, Money::uah(30'000), true));
            storage.seed(BanknoteSet{}.with(Denomination::UAH100, 200));
        }

        Money balance(AccountId id) {
            return storage.accounts().find(id).value().balance();
        }
    };

    TEST(WithdrawalServiceTest, ConstructorRejectsBadSettingsAndEmptyClock) {
        TestStorage storage;
        AtmConfig config;
        const Clock clock = [] { return TimePoint{}; };
        EXPECT_DOMAIN_ERROR((void)WithdrawalService(storage, config, Clock{}), ErrorReason::InvalidData);

        config.dailyWithdrawalLimit = Money{};
        EXPECT_DOMAIN_ERROR((void)WithdrawalService(storage, config, clock), ErrorReason::InvalidData);
        config.dailyWithdrawalLimit = Money::uah(10'000);

        for (const auto amount : { Money{}, Money::uah(150), Money::ofKopiykas(10'001) }) {
            config.quickAmounts = { Money::uah(200), amount };
            EXPECT_DOMAIN_ERROR((void)WithdrawalService(storage, config, clock), ErrorReason::InvalidData);
        }
    }

    TEST(WithdrawalServiceTest, QuickAmountsReturnsAnIndependentCopy) {
        Fixture f;
        auto amounts = f.service.quickAmounts();
        EXPECT_EQ(amounts, f.config.quickAmounts);
        amounts.clear();
        EXPECT_EQ(f.service.quickAmounts(), f.config.quickAmounts);
    }

    TEST(WithdrawalServiceTest, W1WritesBalanceCashAndReturnedJournalEntry) {
        Fixture f;
        const auto entry = f.service.withdraw(f.client, 1, Money::uah(600));

        EXPECT_EQ(f.balance(1), Money::uah(29'400));
        EXPECT_EQ(f.storage.cash().load().count(Denomination::UAH100), 194U);
        EXPECT_TRUE(entry.id > 0);
        EXPECT_EQ(entry.kind, EntryKind::Withdrawal);
        EXPECT_EQ(entry.actor, f.client);
        EXPECT_EQ(entry.account, std::optional<AccountId>{1});
        EXPECT_EQ(entry.amount, Money::uah(600));
        EXPECT_EQ(entry.cash, BanknoteSet{}.with(Denomination::UAH100, 6));
        EXPECT_EQ(entry.atUtc, toEpochSeconds(f.now));
        EXPECT_EQ(entry.localDay, localDay(f.now));
        EXPECT_EQ(f.storage.journal().last(2), std::vector<JournalEntry>{entry});
        EXPECT_EQ(f.clockCalls, 1);
        EXPECT_EQ(f.storage.commits, 1);
        EXPECT_EQ(f.storage.rollbacks, 0);
    }

    TEST(WithdrawalServiceTest, W1RejectsInvalidAmountsWithoutReadingClock) {
        Fixture f;
        const auto beforeCash = f.storage.cash().load();
        for (const auto amount : { Money{}, Money::uah(150), Money::ofKopiykas(10'001) }) {
            EXPECT_DOMAIN_ERROR((void)f.service.withdraw(f.client, 1, amount), ErrorReason::InvalidAmount);
        }
        EXPECT_EQ(f.clockCalls, 0);
        EXPECT_EQ(f.balance(1), Money::uah(30'000));
        EXPECT_EQ(f.storage.cash().load(), beforeCash);
        EXPECT_TRUE(f.storage.journal().last(1).empty());
    }

    TEST(WithdrawalServiceTest, M2RejectsWrongOwnerRoleAndRetainedClient) {
        Fixture f;
        const auto operatorCard = CardNumber::parse("4000000000000003");
        const auto retained = CardNumber::parse("4000000000000004");
        f.storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
        f.storage.seed(Card(retained, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));
        // Synthetic accounts isolate the role/status checks from the ownership check.
        const auto initialBalance = Money::uah(30'000);
        f.storage.seed(Account(4, operatorCard, initialBalance));
        f.storage.seed(Account(5, retained, initialBalance));

        const auto beforeCash = f.storage.cash().load();
        const auto amount = Money::uah(100);
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(f.client, 3, amount), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(operatorCard, 4, amount), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(retained, 5, amount), ErrorReason::AccessDenied);
        EXPECT_EQ(f.balance(3), initialBalance);
        EXPECT_EQ(f.balance(4), initialBalance);
        EXPECT_EQ(f.balance(5), initialBalance);
        EXPECT_EQ(f.storage.cash().load(), beforeCash);
        EXPECT_TRUE(f.storage.journal().last(1).empty());
        EXPECT_EQ(f.clockCalls, 0);
    }

    TEST(WithdrawalServiceTest, DistinguishesUnknownCardAndUnknownAccount) {
        Fixture f;
        const auto missing = CardNumber::parse("4000000000000009");
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(missing, 1, Money::uah(100)), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(f.client, 9, Money::uah(100)), ErrorReason::UnknownAccount);
    }

    TEST(WithdrawalServiceTest, W2ChecksBalanceThenDailyLimitThenCash) {
        Fixture f;
        (void)f.service.withdraw(f.client, 1, Money::uah(10'000));
        f.storage.seed(BanknoteSet{});
        f.storage.seed(Account(2, f.client, Money{}));

        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(f.client, 2, Money::uah(100)),
            ErrorReason::InsufficientFunds);
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(f.client, 1, Money::uah(100)),
            ErrorReason::DailyLimitExceeded);
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(f.other, 3, Money::uah(100)),
            ErrorReason::AtmOutOfCash);

        EXPECT_EQ(f.balance(1), Money::uah(20'000));
        EXPECT_EQ(f.balance(2), Money{});
        EXPECT_EQ(f.balance(3), Money::uah(30'000));
        EXPECT_EQ(f.storage.cash().load(), BanknoteSet{});
        EXPECT_EQ(f.storage.journal().last(10).size(), std::size_t{ 1 });
    }

    TEST(WithdrawalServiceTest, W4LimitIsSharedAcrossAccountsButNotCardsOrDays) {
        Fixture f;
        (void)f.service.withdraw(f.client, 1, Money::uah(6'000));
        (void)f.service.withdraw(f.client, 2, Money::uah(4'000));
        EXPECT_DOMAIN_ERROR((void)f.service.withdraw(f.client, 2, Money::uah(100)),
            ErrorReason::DailyLimitExceeded);
        (void)f.service.withdraw(f.other, 3, Money::uah(100));

        const auto previousDay = localDay(f.now);
        f.now += std::chrono::seconds(1);
        const auto next = f.service.withdraw(f.client, 2, Money::uah(100));
        EXPECT_NE(next.localDay, previousDay);
        EXPECT_EQ(next.localDay, localDay(f.now));
        EXPECT_EQ(next.atUtc, toEpochSeconds(f.now));
        EXPECT_EQ(f.clockCalls, 5);
    }

    TEST(WithdrawalServiceTest, W4ReadsClockOnceEvenWhenItWouldCrossMidnight) {
        Fixture f;
        int calls = 0;
        WithdrawalService service(f.storage, f.config, [&] {
            return f.now + std::chrono::seconds(calls++);
            });
        const auto entry = service.withdraw(f.client, 1, Money::uah(100));
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(entry.atUtc, toEpochSeconds(f.now));
        EXPECT_EQ(entry.localDay, localDay(f.now));
    }

    TEST(WithdrawalServiceTest, DailyLimitComparisonDoesNotOverflowMoney) {
        Fixture f;
        const auto maximum = (std::numeric_limits<std::int64_t>::max)();
        AtmConfig config;
        config.dailyWithdrawalLimit = Money::ofKopiykas(maximum);
        // The largest representable sum of amounts divisible by 100 UAH.
        f.storage.withdrawnTodayOverride = Money::ofKopiykas(maximum / 10'000 * 10'000);

        WithdrawalService service(f.storage, config, [&] { return f.now; });
        EXPECT_DOMAIN_ERROR((void)service.withdraw(f.client, 1, Money::uah(100)),
            ErrorReason::DailyLimitExceeded);
        EXPECT_EQ(f.balance(1), Money::uah(30'000));
    }

    TEST(WithdrawalServiceTest, X1RestoresStateOnAppendAndCommitFailure) {
        for (const auto failure : { TestStorage::Failure::Append, TestStorage::Failure::Commit }) {
            Fixture f;
            (void)f.service.withdraw(f.client, 1, Money::uah(100));
            const auto balanceBefore = f.balance(1);
            const auto cashBefore = f.storage.cash().load();
            const auto journalBefore = f.storage.journal().last(10);

            f.storage.failure = failure;
            EXPECT_THROW((void)f.service.withdraw(f.client, 1, Money::uah(600)), InjectedStorageFailure);
            EXPECT_EQ(f.balance(1), balanceBefore);
            EXPECT_EQ(f.storage.cash().load(), cashBefore);
            EXPECT_EQ(f.storage.journal().last(10), journalBefore);
            EXPECT_EQ(f.storage.commits, 1);
            EXPECT_EQ(f.storage.rollbacks, 1);

            f.storage.failure = TestStorage::Failure::None;
            const auto recovered = f.service.withdraw(f.client, 1, Money::uah(600));
            EXPECT_EQ(recovered.id, journalBefore.front().id + 1);
        }
    }

    TEST(ApplicationAccessTest, OperatorCheckAcceptsOnlyExistingServiceCards) {
        Fixture f;
        const auto operatorCard = CardNumber::parse("4000000000000003");
        const auto missing = CardNumber::parse("4000000000000009");
        f.storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
        EXPECT_EQ(atm::application::detail::requireOperator(f.storage, operatorCard).number(),
            operatorCard);
        EXPECT_DOMAIN_ERROR((void)atm::application::detail::requireOperator(f.storage, f.client),
            ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)atm::application::detail::requireOperator(f.storage, missing),
            ErrorReason::UnknownCard);
    }

}  // namespace

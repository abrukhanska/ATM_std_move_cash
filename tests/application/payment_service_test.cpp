#include "application/payment_service.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/withdrawal_service.hpp"
#include "domain/account.hpp"
#include "domain/atm_config.hpp"
#include "domain/banknote_set.hpp"
#include "domain/card.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using namespace atm::domain;
    using atm::application::PaymentService;
    using atm::application::WithdrawalService;
    using atm::test_support::InjectedStorageFailure;
    using atm::test_support::TestStorage;

    constexpr std::int64_t kMobile = 1;
    constexpr std::int64_t kCharity = 2;
    constexpr std::string_view kPhone = "0501234567";

    struct Fixture {
        TestStorage storage;
        const CardNumber client = CardNumber::parse("4000000000000001");
        const CardNumber other = CardNumber::parse("4000000000000002");
        const TimePoint now = fromEpochSeconds(1'768'471'200);
        const BanknoteSet cash = BanknoteSet{}.with(Denomination::UAH100, 100);
        int clockCalls = 0;
        PaymentService service{ storage, [this] {
            ++clockCalls;
            return now;
        } };

        Fixture() {
            storage.cards().save(Card(client, "Client", "hash", CardKind::Client));
            storage.cards().save(Card(other, "Other", "hash", CardKind::Client));
            storage.accounts().save(Account(1, client, Money::uah(30'000), true));
            storage.accounts().save(Account(2, client, Money::uah(1'000)));
            storage.accounts().save(Account(3, other, Money::uah(2'000), true));
            storage.cash().save(cash);
        }

        Money balance(AccountId id) {
            return storage.accounts().find(id).value().balance();
        }

        void expectUnchanged() {
            EXPECT_EQ(balance(1), Money::uah(30'000));
            EXPECT_EQ(balance(2), Money::uah(1'000));
            EXPECT_EQ(balance(3), Money::uah(2'000));
            EXPECT_EQ(storage.cash().load(), cash);
            EXPECT_TRUE(storage.journal().last(1).empty());
            EXPECT_EQ(storage.commits, 0);
            EXPECT_EQ(clockCalls, 0);
        }
    };

    TEST(PaymentServiceTest, DefaultDirectoryOffersOperatorsAndCharities) {
        Fixture f;
        const auto mobile = f.service.payees(PayeeCategory::MobileOperator);
        const auto charity = f.service.payees(PayeeCategory::Charity);
        ASSERT_EQ(mobile.size(), std::size_t{ 3 });
        ASSERT_EQ(charity.size(), std::size_t{ 2 });
        EXPECT_EQ(mobile[0].id, kMobile);
        EXPECT_EQ(mobile[0].name, "Vodafone Ukraine");
        EXPECT_EQ(mobile[1].id, 3);
        EXPECT_EQ(mobile[1].name, "Kyivstar");
        EXPECT_EQ(mobile[2].id, 4);
        EXPECT_EQ(mobile[2].name, "lifecell");
        EXPECT_EQ(charity[0].id, kCharity);
        EXPECT_EQ(charity[0].name, "Come Back Alive");
        EXPECT_EQ(charity[1].id, 5);
        EXPECT_EQ(charity[1].name, "UNITED24");
        f.expectUnchanged();
    }

    TEST(PaymentServiceTest, MobilePaymentUsesSelectedOperatorWithoutRequiringItsPrefix) {
        Fixture f;
        // A number starting with 050 may have been ported to Kyivstar.
        const auto entry = f.service.pay(f.client, 2, 3, kPhone, Money::uah(10));
        EXPECT_EQ(entry.payeeId, std::optional<std::int64_t>{ 3 });
        EXPECT_EQ(std::string_view(entry.requisite), kPhone);
        EXPECT_EQ(f.balance(2), Money::uah(990));
        const auto journal = f.storage.journal().last(2);
        ASSERT_EQ(journal.size(), std::size_t{ 1 });
        EXPECT_EQ(journal.front(), entry);
    }

    TEST(PaymentServiceTest, CustomDirectoryIsFilteredAndReturnedByValue) {
        Fixture f;
        PaymentService service(f.storage, [&f] { return f.now; }, {
            { 12, "First", PayeeCategory::Charity },
            { 11, "Mobile", PayeeCategory::MobileOperator },
            { 10, "Second", PayeeCategory::Charity }
            });
        auto charity = service.payees(PayeeCategory::Charity);
        ASSERT_EQ(charity.size(), std::size_t{ 2 });
        EXPECT_EQ(charity[0].id, 12);
        EXPECT_EQ(charity[1].id, 10);
        charity.clear();
        EXPECT_EQ(service.payees(PayeeCategory::Charity).size(), std::size_t{ 2 });
        const auto entry = service.pay(f.client, 2, 12, "", Money::uah(10));
        EXPECT_EQ(entry.payeeId, std::optional<std::int64_t>{ 12 });
        EXPECT_EQ(f.balance(2), Money::uah(990));
    }

    TEST(PaymentServiceTest, ConstructorRejectsEmptyClockAndInvalidDirectoryEntries) {
        Fixture f;
        EXPECT_DOMAIN_ERROR((void)PaymentService(f.storage, Clock{}), ErrorReason::InvalidData);

        const std::vector<std::vector<Payee>> invalid{
            {{ 1, "A", PayeeCategory::Charity }, { 1, "B", PayeeCategory::MobileOperator }},
            {{ 0, "A", PayeeCategory::Charity }},
            {{ -3, "A", PayeeCategory::Charity }},
            {{ 1, "", PayeeCategory::Charity }},
            {{ 1, "A", static_cast<PayeeCategory>(99) }}
        };
        for (const auto& directory : invalid) {
            EXPECT_DOMAIN_ERROR((void)PaymentService(f.storage, [&f] { return f.now; }, directory),
                ErrorReason::InvalidData);
        }
        f.expectUnchanged();
    }

    TEST(PaymentServiceTest, P1MobilePaymentDebitsSelectedAccountAndRecordsExactDetails) {
        Fixture f;
        const auto amount = Money::ofKopiykas(12'345);
        const auto entry = f.service.pay(f.client, 2, kMobile, kPhone, amount);

        EXPECT_EQ(f.balance(1), Money::uah(30'000));
        EXPECT_EQ(f.balance(2), Money::ofKopiykas(87'655));
        EXPECT_EQ(f.balance(3), Money::uah(2'000));
        EXPECT_EQ(f.storage.cash().load(), f.cash);
        EXPECT_TRUE(entry.id > 0);
        EXPECT_EQ(entry.kind, EntryKind::Payment);
        EXPECT_EQ(entry.actor, f.client);
        EXPECT_EQ(entry.account, std::optional<AccountId>{ 2 });
        EXPECT_FALSE(entry.target.has_value());
        EXPECT_FALSE(entry.subjectCard.has_value());
        EXPECT_EQ(entry.amount, amount);
        EXPECT_EQ(entry.cash, BanknoteSet{});
        EXPECT_EQ(entry.payeeId, std::optional<std::int64_t>{ kMobile });
        EXPECT_EQ(std::string_view(entry.requisite), kPhone);
        EXPECT_EQ(entry.atUtc, 1'768'471'200);
        EXPECT_EQ(entry.localDay, localDay(f.now));
        EXPECT_EQ(f.clockCalls, 1);
        EXPECT_EQ(f.storage.commits, 1);
        EXPECT_EQ(f.storage.rollbacks, 0);
        const auto journal = f.storage.journal().last(2);
        ASSERT_EQ(journal.size(), std::size_t{ 1 });
        EXPECT_EQ(journal.front(), entry);
    }

    TEST(PaymentServiceTest, MobilePaymentCopiesOnlyTheRequisiteView) {
        Fixture f;
        std::string input = "0501234567 trailing text";
        const std::string_view phone{ input.data(), kPhone.size() };
        const auto entry = f.service.pay(f.client, 2, kMobile, phone, Money::uah(10));

        input[0] = '9';
        EXPECT_EQ(std::string_view(entry.requisite), kPhone);
        const auto journal = f.storage.journal().last(1);
        ASSERT_EQ(journal.size(), std::size_t{ 1 });
        EXPECT_EQ(std::string_view(journal.front().requisite), kPhone);
    }

    TEST(PaymentServiceTest, P1CharityAcceptsEmptyRequisiteAndWholeBalance) {
        Fixture f;
        const auto entry = f.service.pay(f.client, 2, kCharity, "", Money::uah(1'000));
        EXPECT_EQ(f.balance(2), Money{});
        EXPECT_EQ(entry.kind, EntryKind::Payment);
        EXPECT_EQ(entry.amount, Money::uah(1'000));
        EXPECT_EQ(entry.payeeId, std::optional<std::int64_t>{ kCharity });
        EXPECT_TRUE(entry.requisite.empty());
        EXPECT_EQ(f.storage.cash().load(), f.cash);
        const auto journal = f.storage.journal().last(2);
        ASSERT_EQ(journal.size(), std::size_t{ 1 });
        EXPECT_EQ(journal.front(), entry);
    }

    TEST(PaymentServiceTest, P1RejectsMalformedRequisitesWithoutChanges) {
        Fixture f;
        constexpr std::string_view bad[]{
            "", "050123456", "05012345678", "050123456a", "+380501234567",
            "050 123 4567", "050-123-4567", " 501234567", "0501234567 ",
            "05012345\xD9\xA7", std::string_view{ "05012\0" "4567", 10 }
        };
        for (const auto requisite : bad) {
            EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, kMobile, requisite, Money::uah(10)),
                ErrorReason::InvalidRequisite);
        }
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, kCharity, kPhone, Money::uah(10)),
            ErrorReason::InvalidRequisite);
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, kCharity, " ", Money::uah(10)),
            ErrorReason::InvalidRequisite);
        f.expectUnchanged();
        EXPECT_EQ(f.storage.rollbacks, 0);
    }

    TEST(PaymentServiceTest, P1RejectsUnknownPayeeZeroAndInsufficientFunds) {
        Fixture f;
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, 999, kPhone, Money::uah(10)),
            ErrorReason::UnknownPayee);
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, kMobile, kPhone, Money{}),
            ErrorReason::InvalidAmount);
        EXPECT_EQ(f.storage.rollbacks, 0);

        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, kMobile, kPhone, Money::ofKopiykas(100'001)),
            ErrorReason::InsufficientFunds);
        f.expectUnchanged();
        EXPECT_EQ(f.storage.rollbacks, 1);
    }

    TEST(PaymentServiceTest, M2RejectsUnauthorizedPayers) {
        Fixture f;
        const auto missing = CardNumber::parse("4000000000000009");
        const auto amount = Money::uah(10);
        EXPECT_DOMAIN_ERROR((void)f.service.pay(missing, 2, kCharity, "", amount), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 9, kCharity, "", amount), ErrorReason::UnknownAccount);
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 3, kCharity, "", amount), ErrorReason::AccessDenied);

        f.storage.cards().save(Card(f.client, "Client", "hash", CardKind::Service));
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, kCharity, "", amount), ErrorReason::AccessDenied);
        f.storage.cards().save(Card(f.client, "Client", "hash", CardKind::Client, CardStatus::Retained, 3));
        EXPECT_DOMAIN_ERROR((void)f.service.pay(f.client, 2, kCharity, "", amount), ErrorReason::AccessDenied);
        f.expectUnchanged();
    }

    TEST(PaymentServiceTest, PaymentDoesNotConsumeDailyWithdrawalLimit) {
        Fixture f;
        (void)f.service.pay(f.client, 1, kCharity, "", Money::uah(9'000));
        const AtmConfig config;
        WithdrawalService withdrawal(f.storage, config, [&f] { return f.now; });
        const auto entry = withdrawal.withdraw(f.client, 1, Money::uah(10'000));
        EXPECT_EQ(entry.kind, EntryKind::Withdrawal);
        EXPECT_EQ(f.balance(1), Money::uah(11'000));
        EXPECT_EQ(f.storage.commits, 2);
    }

    TEST(PaymentServiceTest, X1RestoresBalanceAndJournalOnAppendAndCommitFailure) {
        for (const auto failure : { TestStorage::Failure::Append, TestStorage::Failure::Commit }) {
            Fixture f;
            const auto amount = Money::uah(100);
            const auto first = f.service.pay(f.client, 2, kMobile, kPhone, amount);
            const auto journal = f.storage.journal().last(2);
            ASSERT_EQ(journal.size(), std::size_t{ 1 });
            EXPECT_EQ(journal.front(), first);

            f.storage.failure = failure;
            EXPECT_THROW((void)f.service.pay(f.client, 2, kMobile, kPhone, amount), InjectedStorageFailure);
            EXPECT_EQ(f.balance(2), Money::uah(900));
            EXPECT_EQ(f.storage.journal().last(2), journal);
            EXPECT_EQ(f.storage.cash().load(), f.cash);
            EXPECT_EQ(f.storage.commits, 1);
            EXPECT_EQ(f.storage.rollbacks, 1);

            f.storage.failure = TestStorage::Failure::None;
            (void)f.service.pay(f.client, 2, kMobile, kPhone, amount);
            EXPECT_EQ(f.balance(2), Money::uah(800));
            EXPECT_EQ(f.storage.commits, 2);
        }
    }

}  // namespace

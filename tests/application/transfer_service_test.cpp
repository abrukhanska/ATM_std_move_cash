#include "application/transfer_service.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "domain/account.hpp"
#include "domain/card.hpp"
#include "domain/enums.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using namespace atm::domain;
    using atm::application::TransferService;
    using atm::test_support::InjectedStorageFailure;
    using atm::test_support::TestStorage;

    struct Fixture {
        TestStorage storage;
        const CardNumber sender = CardNumber::parse("4000000000000001");
        const CardNumber recipient = CardNumber::parse("4000000000000002");
        const TimePoint now = fromEpochSeconds(1'768'471'200);
        int clockCalls = 0;
        TransferService service{ storage, [this] {
            ++clockCalls;
            return now;
        } };

        Fixture() {
            storage.cards().save(Card(sender, "Sender", "hash", CardKind::Client));
            storage.cards().save(Card(recipient, "Recipient", "hash", CardKind::Client));
            storage.accounts().save(Account(1, sender, Money::uah(2'000), true));
            storage.accounts().save(Account(2, sender, Money::uah(1'000)));
            storage.accounts().save(Account(3, recipient, Money::uah(300)));
            storage.accounts().save(Account(4, recipient, Money::uah(500), true));
            storage.cash().save(BanknoteSet{}.with(Denomination::UAH100, 3));
        }

        Money balance(AccountId id) {
            return storage.accounts().find(id).value().balance();
        }

        void expectUnchanged(Money from = Money::uah(1'000), Money to = Money::uah(500)) {
            EXPECT_EQ(balance(1), Money::uah(2'000));
            EXPECT_EQ(balance(2), from);
            EXPECT_EQ(balance(3), Money::uah(300));
            EXPECT_EQ(balance(4), to);
            EXPECT_EQ(storage.cash().load(), BanknoteSet{}.with(Denomination::UAH100, 3));
            EXPECT_TRUE(storage.journal().last(1).empty());
            EXPECT_EQ(storage.commits, 0);
            EXPECT_EQ(clockCalls, 0);
        }
    };

    TEST(TransferServiceTest, ConstructorRejectsEmptyClock) {
        TestStorage storage;
        EXPECT_DOMAIN_ERROR((void)TransferService(storage, Clock{}), ErrorReason::InvalidData);
    }

    TEST(TransferServiceTest, TR1TransfersExactAmountToActiveAndRetainedRecipients) {
        for (const auto status : { CardStatus::Active, CardStatus::Retained }) {
            Fixture f;
            if (status == CardStatus::Retained) {
                f.storage.cards().save(Card(f.recipient, "Recipient", "hash", CardKind::Client, status, 3));
            }
            const auto amount = Money::ofKopiykas(12'345);
            const auto preview = f.service.preview(f.sender, 2, f.recipient, amount);
            EXPECT_EQ(preview.toCard, f.recipient);
            EXPECT_EQ(preview.toAccount, 4);
            EXPECT_EQ(preview.recipientName, "Recipient");
            EXPECT_EQ(preview.amount, amount);
            f.expectUnchanged();
            EXPECT_EQ(f.storage.rollbacks, 0);

            const auto entry = f.service.execute(f.sender, 2, preview);
            EXPECT_EQ(f.balance(1), Money::uah(2'000));
            EXPECT_EQ(f.balance(2), Money::ofKopiykas(87'655));
            EXPECT_EQ(f.balance(3), Money::uah(300));
            EXPECT_EQ(f.balance(4), Money::ofKopiykas(62'345));
            EXPECT_EQ(f.storage.cards().find(f.recipient).value().status(), status);
            EXPECT_EQ(f.storage.cash().load(), BanknoteSet{}.with(Denomination::UAH100, 3));
            EXPECT_TRUE(entry.id > 0);
            EXPECT_EQ(entry.kind, EntryKind::Transfer);
            EXPECT_EQ(entry.actor, f.sender);
            EXPECT_EQ(entry.account, std::optional<AccountId>{2});
            EXPECT_EQ(entry.target, std::optional<AccountId>{4});
            EXPECT_EQ(entry.amount, amount);
            EXPECT_EQ(entry.cash, BanknoteSet{});
            EXPECT_EQ(entry.atUtc, 1'768'471'200);
            EXPECT_EQ(entry.localDay, localDay(f.now));
            EXPECT_EQ(f.clockCalls, 1);
            EXPECT_EQ(f.storage.commits, 1);
            EXPECT_EQ(f.storage.rollbacks, 0);
            const auto journal = f.storage.journal().last(2);
            ASSERT_EQ(journal.size(), std::size_t{ 1 });
            EXPECT_EQ(journal.front(), entry);
        }
    }

    TEST(TransferServiceTest, TR2RejectsZeroAndOwnCardInBothSteps) {
        Fixture f;
        const auto amount = Money::uah(100);
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 2, f.recipient, Money{}),
            ErrorReason::InvalidAmount);
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 2, f.sender, amount), ErrorReason::SelfTransfer);

        auto preview = f.service.preview(f.sender, 2, f.recipient, amount);
        preview.amount = Money{};
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::InvalidAmount);
        preview.amount = amount;
        preview.toCard = f.sender;
        preview.toAccount = 1;
        preview.recipientName = "Sender";
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::SelfTransfer);
        f.expectUnchanged();
    }

    TEST(TransferServiceTest, TR2RejectsUnknownServiceAndMissingPrimaryRecipients) {
        Fixture f;
        const auto missing = CardNumber::parse("4000000000000009");
        const auto operatorCard = CardNumber::parse("4000000000000003");
        f.storage.cards().save(Card(operatorCard, "Operator", "hash", CardKind::Service));
        f.storage.accounts().save(Account(5, operatorCard, Money{}, true));
        const auto amount = Money::uah(100);
        auto preview = f.service.preview(f.sender, 2, f.recipient, amount);

        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 2, missing, amount), ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 2, operatorCard, amount), ErrorReason::UnknownCard);

        preview.toCard = missing;
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::InvalidPreview);
        preview.toCard = operatorCard;
        preview.toAccount = 5;
        preview.recipientName = "Operator";
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::InvalidPreview);

        preview = f.service.preview(f.sender, 2, f.recipient, amount);
        f.storage.accounts().save(Account(4, f.recipient, Money::uah(500)));
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 2, f.recipient, amount),
            ErrorReason::UnknownAccount);
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::InvalidPreview);
        EXPECT_EQ(f.balance(5), Money{});
        f.expectUnchanged();
    }

    TEST(TransferServiceTest, TR2RejectsStaleRecipientNameAndPrimaryAccount) {
        Fixture f;
        const auto old = f.service.preview(f.sender, 2, f.recipient, Money::uah(100));
        f.storage.cards().save(Card(f.recipient, "Renamed", "hash", CardKind::Client));
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, old), ErrorReason::InvalidPreview);

        const auto refreshed = f.service.preview(f.sender, 2, f.recipient, Money::uah(100));
        f.storage.accounts().save(Account(4, f.recipient, Money::uah(500)));
        f.storage.accounts().save(Account(3, f.recipient, Money::uah(300), true));
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, refreshed), ErrorReason::InvalidPreview);
        f.expectUnchanged();
    }

    TEST(TransferServiceTest, PreviewRejectsUnauthorizedSenderWithoutChangingState) {
        Fixture f;
        const auto missing = CardNumber::parse("4000000000000009");
        const auto amount = Money::uah(100);

        EXPECT_DOMAIN_ERROR((void)f.service.preview(missing, 2, f.recipient, amount),
            ErrorReason::UnknownCard);
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 9, f.recipient, amount),
            ErrorReason::UnknownAccount);
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 3, f.recipient, amount),
            ErrorReason::AccessDenied);

        f.storage.cards().save(Card(f.sender, "Sender", "hash", CardKind::Service));
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 2, f.recipient, amount),
            ErrorReason::AccessDenied);
        f.storage.cards().save(Card(f.sender, "Sender", "hash", CardKind::Client, CardStatus::Retained, 3));
        EXPECT_DOMAIN_ERROR((void)f.service.preview(f.sender, 2, f.recipient, amount),
            ErrorReason::AccessDenied);
        f.expectUnchanged();
    }

    TEST(TransferServiceTest, AcceptsExactBalanceAndRecipientMaximum) {
        Fixture f;
        const auto maximum = (std::numeric_limits<std::int64_t>::max)();
        const auto amount = Money::ofKopiykas(1);
        f.storage.accounts().save(Account(2, f.sender, amount));
        f.storage.accounts().save(Account(4, f.recipient, Money::ofKopiykas(maximum - 1), true));

        const auto preview = f.service.preview(f.sender, 2, f.recipient, amount);
        const auto entry = f.service.execute(f.sender, 2, preview);
        EXPECT_EQ(f.balance(2), Money{});
        EXPECT_EQ(f.balance(4), Money::ofKopiykas(maximum));
        EXPECT_EQ(entry.amount, amount);
        EXPECT_EQ(f.storage.commits, 1);
        EXPECT_EQ(f.storage.rollbacks, 0);
    }

    TEST(TransferServiceTest, ExecuteRevalidatesSenderAfterPreview) {
        Fixture f;
        const auto preview = f.service.preview(f.sender, 2, f.recipient, Money::uah(100));
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 9, preview), ErrorReason::UnknownAccount);
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 3, preview), ErrorReason::AccessDenied);
        f.storage.cards().save(Card(f.sender, "Sender", "hash", CardKind::Client, CardStatus::Retained, 3));
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::AccessDenied);
        f.expectUnchanged();
    }

    TEST(TransferServiceTest, TR2RejectsInsufficientFundsAndRecipientOverflow) {
        Fixture f;
        const auto preview = f.service.preview(f.sender, 2, f.recipient, Money::uah(100));
        f.storage.accounts().save(Account(2, f.sender, Money::uah(99)));
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::InsufficientFunds);
        f.expectUnchanged(Money::uah(99));

        const auto maximum = Money::ofKopiykas((std::numeric_limits<std::int64_t>::max)());
        f.storage.accounts().save(Account(2, f.sender, Money::uah(1'000)));
        f.storage.accounts().save(Account(4, f.recipient, maximum, true));
        EXPECT_DOMAIN_ERROR((void)f.service.execute(f.sender, 2, preview), ErrorReason::Overflow);
        f.expectUnchanged(Money::uah(1'000), maximum);
    }

    TEST(TransferServiceTest, X1RestoresBothBalancesAndJournalAfterFailure) {
        for (const auto failure : { TestStorage::Failure::Append, TestStorage::Failure::Commit }) {
            Fixture f;
            const auto preview = f.service.preview(f.sender, 2, f.recipient, Money::uah(100));
            const auto first = f.service.execute(f.sender, 2, preview);
            const auto journal = f.storage.journal().last(2);
            ASSERT_EQ(journal.size(), std::size_t{ 1 });
            EXPECT_EQ(journal.front(), first);

            f.storage.failure = failure;
            EXPECT_THROW((void)f.service.execute(f.sender, 2, preview), InjectedStorageFailure);
            EXPECT_EQ(f.balance(2), Money::uah(900));
            EXPECT_EQ(f.balance(4), Money::uah(600));
            EXPECT_EQ(f.storage.journal().last(2), journal);
            EXPECT_EQ(f.storage.commits, 1);
            EXPECT_EQ(f.storage.rollbacks, 1);

            f.storage.failure = TestStorage::Failure::None;
            (void)f.service.execute(f.sender, 2, preview);
            EXPECT_EQ(f.balance(2), Money::uah(800));
            EXPECT_EQ(f.balance(4), Money::uah(700));
            EXPECT_EQ(f.storage.commits, 2);
        }
    }

}  // namespace

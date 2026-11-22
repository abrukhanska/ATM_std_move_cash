#include <cstdint>
#include <vector>

#include "application/card_administration_service.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using atm::application::CardAdministrationService;
    using atm::domain::Card;
    using atm::domain::CardKind;
    using atm::domain::CardNumber;
    using atm::domain::CardStatus;
    using atm::domain::DomainError;
    using atm::domain::EntryKind;
    using atm::domain::ErrorReason;
    using atm::domain::JournalEntry;
    using atm::test_support::InjectedStorageFailure;
    using atm::test_support::TestStorage;

    TEST(SV3CardAdmin, ReleasesRetainedCardAndLogsEntry) {
        TestStorage storage;
        const auto operatorCard = CardNumber::parse("4000000000000001");
        const auto retainedCard = CardNumber::parse("4000000000000002");

        storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
        storage.seed(Card(retainedCard, "Client", "hash", CardKind::Client, CardStatus::Retained, 3));

        int clockCalls = 0;
        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        CardAdministrationService service(storage, [&] { ++clockCalls; return now; });

        const JournalEntry entry = service.release(operatorCard, retainedCard);

        const Card updated = storage.cards().find(retainedCard).value();
        EXPECT_EQ(updated.status(), CardStatus::Active);
        EXPECT_EQ(updated.failedAttempts(), std::uint8_t{ 0 });

        EXPECT_EQ(entry.kind, EntryKind::CardReleased);
        EXPECT_EQ(entry.actor, operatorCard);
        ASSERT_TRUE(entry.subjectCard.has_value());
        EXPECT_EQ(*entry.subjectCard, retainedCard);
        EXPECT_EQ(entry.atUtc, atm::domain::toEpochSeconds(now));
        EXPECT_EQ(entry.localDay, atm::domain::localDay(now));

        const auto entries = storage.journal().last(20);
        ASSERT_EQ(entries.size(), std::size_t{ 1 });
        EXPECT_EQ(entries[0], entry);
        EXPECT_EQ(storage.commits, 1);
        EXPECT_EQ(clockCalls, 1);
    }

    TEST(SV3CardAdmin, ListsOnlyRetainedCardsWithoutWrites) {
        TestStorage storage;
        const auto operatorCard = CardNumber::parse("4000000000000001");
        const auto activeCard = CardNumber::parse("4000000000000002");
        const auto retainedCard = CardNumber::parse("4000000000000003");

        storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
        storage.seed(Card(activeCard, "Active", "hash", CardKind::Client));
        storage.seed(Card(retainedCard, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));

        int clockCalls = 0;
        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        CardAdministrationService service(storage, [&] { ++clockCalls; return now; });

        const std::vector<Card> retained = service.listRetained(operatorCard);
        ASSERT_EQ(retained.size(), std::size_t{ 1 });
        EXPECT_EQ(retained[0].number(), retainedCard);
        EXPECT_EQ(storage.commits, 0);
        EXPECT_EQ(clockCalls, 0);
    }

    TEST(SV3CardAdmin, RollsBackCardChangeOnStorageFailure) {
        for (const auto failure : { TestStorage::Failure::Append, TestStorage::Failure::Commit }) {
            TestStorage storage;
            const auto operatorCard = CardNumber::parse("4000000000000001");
            const auto retainedCard = CardNumber::parse("4000000000000002");

            storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
            storage.seed(Card(retainedCard, "Client", "hash", CardKind::Client, CardStatus::Retained, 3));

            const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
            CardAdministrationService service(storage, [&] { return now; });

            storage.failure = failure;
            EXPECT_THROW((void)service.release(operatorCard, retainedCard), InjectedStorageFailure);

            const Card unchanged = storage.cards().find(retainedCard).value();
            EXPECT_EQ(unchanged.status(), CardStatus::Retained);
            EXPECT_EQ(unchanged.failedAttempts(), std::uint8_t{ 3 });
            EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
            EXPECT_EQ(storage.rollbacks, 1);
        }
    }

    TEST(SV3CardAdmin, RejectsNonServiceActorAndUnknownTarget) {
        TestStorage storage;
        const auto operatorCard = CardNumber::parse("4000000000000001");
        const auto clientCard = CardNumber::parse("4000000000000002");
        const auto retainedCard = CardNumber::parse("4000000000000003");
        const auto unknownCard = CardNumber::parse("9000000000009999");

        storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
        storage.seed(Card(clientCard, "Active", "hash", CardKind::Client));
        storage.seed(Card(retainedCard, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        CardAdministrationService service(storage, [&] { return now; });

        EXPECT_DOMAIN_ERROR((void)service.listRetained(clientCard), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.release(clientCard, retainedCard), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.release(retainedCard, retainedCard), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.release(unknownCard, retainedCard), ErrorReason::UnknownCard);

        EXPECT_DOMAIN_ERROR((void)service.release(operatorCard, unknownCard), ErrorReason::UnknownCard);

        EXPECT_EQ(storage.cards().find(retainedCard).value().status(), CardStatus::Retained);
        EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
    }

    TEST(SV3CardAdmin, RejectsActiveClientAndServiceTargetCards) {
        TestStorage storage;
        const auto operatorCard = CardNumber::parse("4000000000000001");
        const auto activeClientCard = CardNumber::parse("4000000000000002");
        const auto otherServiceCard = CardNumber::parse("4000000000000003");

        storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));
        storage.seed(Card(activeClientCard, "Client", "hash", CardKind::Client, CardStatus::Active, 2));
        storage.seed(Card(otherServiceCard, "Tech", "hash", CardKind::Service));

        int clockCalls = 0;
        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        CardAdministrationService service(storage, [&] { ++clockCalls; return now; });

        EXPECT_DOMAIN_ERROR((void)service.release(operatorCard, activeClientCard), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.release(operatorCard, operatorCard), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.release(operatorCard, otherServiceCard), ErrorReason::AccessDenied);

        const Card unchangedClient = storage.cards().find(activeClientCard).value();
        EXPECT_EQ(unchangedClient.status(), CardStatus::Active);
        EXPECT_EQ(unchangedClient.failedAttempts(), std::uint8_t{ 2 });

        const Card unchangedService = storage.cards().find(operatorCard).value();
        EXPECT_EQ(unchangedService.status(), CardStatus::Active);
        EXPECT_EQ(unchangedService.failedAttempts(), std::uint8_t{ 0 });

        EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
        EXPECT_EQ(clockCalls, 0);
    }

}  // namespace
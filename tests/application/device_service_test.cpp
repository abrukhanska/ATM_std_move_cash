#include "application/device_service.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/enums.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using atm::application::DeviceService;
    using atm::domain::Card;
    using atm::domain::CardKind;
    using atm::domain::CardNumber;
    using atm::domain::CardStatus;
    using atm::domain::EntryKind;
    using atm::domain::ErrorReason;
    using atm::domain::JournalEntry;
    using atm::test_support::InjectedStorageFailure;
    using atm::test_support::TestStorage;

    TEST(SV5Device, SwitchesATMMode) {
        TestStorage storage;
        const auto operatorCard = CardNumber::parse("4000000000000001");
        storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));

        const auto t1 = atm::domain::fromEpochSeconds(1'768'471'200);
        const auto t2 = atm::domain::fromEpochSeconds(1'768'471'500);
        auto currentTime = t1;
        DeviceService service(storage, [&] { return currentTime; });

        EXPECT_TRUE(service.isInService());
        EXPECT_FALSE(service.isBroken());

        const JournalEntry outEntry = service.setInService(operatorCard, false);
        EXPECT_FALSE(service.isInService());
        EXPECT_EQ(outEntry.kind, EntryKind::DeviceOutOfService);
        EXPECT_EQ(outEntry.actor, operatorCard);
        EXPECT_EQ(outEntry.atUtc, atm::domain::toEpochSeconds(t1));
        EXPECT_EQ(outEntry.localDay, atm::domain::localDay(t1));

        currentTime = t2;
        const JournalEntry inEntry = service.setInService(operatorCard, true);
        EXPECT_TRUE(service.isInService());
        EXPECT_EQ(inEntry.kind, EntryKind::DeviceInService);
        EXPECT_EQ(inEntry.actor, operatorCard);
        EXPECT_EQ(inEntry.atUtc, atm::domain::toEpochSeconds(t2));
        EXPECT_EQ(inEntry.localDay, atm::domain::localDay(t2));

        const auto entries = storage.journal().last(100);
        ASSERT_EQ(entries.size(), std::size_t{ 2 });
        EXPECT_EQ(entries[0], inEntry);
        EXPECT_EQ(entries[1], outEntry);
        EXPECT_EQ(storage.commits, 2);
    }

    TEST(SV5Device, KeepsModeOnStorageError) {
        for (const auto failure : { TestStorage::Failure::Append, TestStorage::Failure::Commit }) {
            TestStorage storage;
            const auto operatorCard = CardNumber::parse("4000000000000001");
            storage.seed(Card(operatorCard, "Operator", "hash", CardKind::Service));

            const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
            DeviceService service(storage, [&] { return now; });

            storage.failure = failure;
            EXPECT_THROW((void)service.setInService(operatorCard, false), InjectedStorageFailure);

            EXPECT_TRUE(service.isInService());
            EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 0 });
            EXPECT_EQ(storage.rollbacks, 1);
        }
    }

    TEST(SV5Device, RejectsNonServiceAndUnknownCards) {
        TestStorage storage;
        const auto clientCard = CardNumber::parse("4000000000000002");
        const auto retainedCard = CardNumber::parse("4000000000000003");
        const auto unknownCard = CardNumber::parse("9000000000009999");

        storage.seed(Card(clientCard, "Client", "hash", CardKind::Client));
        storage.seed(Card(retainedCard, "Retained", "hash", CardKind::Client, CardStatus::Retained, 3));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        DeviceService service(storage, [&] { return now; });

        EXPECT_DOMAIN_ERROR((void)service.setInService(clientCard, false), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.setInService(retainedCard, false), ErrorReason::AccessDenied);
        EXPECT_DOMAIN_ERROR((void)service.setInService(unknownCard, false), ErrorReason::UnknownCard);

        EXPECT_TRUE(service.isInService());
        EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
    }

}  // namespace
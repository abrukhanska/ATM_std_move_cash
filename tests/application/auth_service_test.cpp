#include <cstdint>
#include <stdexcept>

#include "application/auth_service.hpp"
#include "application/pin_hasher.hpp"
#include "domain/atm_config.hpp"
#include "domain/card.hpp"
#include "domain/card_number.hpp"
#include "domain/domain_error.hpp"
#include "domain/enums.hpp"
#include "domain/pin_code.hpp"
#include "support/domain_assertions.hpp"
#include "support/mini_test.hpp"
#include "support/transactional_test_storage.hpp"

namespace {

    using atm::application::AuthService;
    using atm::application::PinHasher;
    using atm::domain::AtmConfig;
    using atm::domain::AuthStatus;
    using atm::domain::Card;
    using atm::domain::CardInspection;
    using atm::domain::CardKind;
    using atm::domain::CardNumber;
    using atm::domain::CardStatus;
    using atm::domain::EntryKind;
    using atm::domain::ErrorReason;
    using atm::domain::PinCode;
    using atm::test_support::TestStorage;

    TEST(A1Auth, InspectActiveClientCard) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        storage.seed(Card(clientCard, "Client", hasher.hash("1234"), CardKind::Client));

        int clockCalls = 0;
        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { ++clockCalls; return now; });

        EXPECT_EQ(service.inspect(clientCard), CardInspection::Client);
        EXPECT_EQ(storage.commits, 0);
        EXPECT_EQ(clockCalls, 0);
        EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 0 });
    }

    TEST(A1Auth, CorrectPin) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        storage.seed(Card(clientCard, "Client", hasher.hash("1234"), CardKind::Client));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { return now; });

        EXPECT_EQ(service.authenticate(clientCard, PinCode::parse("1234")), AuthStatus::Ok);

        const Card stored = storage.cards().find(clientCard).value();
        EXPECT_EQ(stored.status(), CardStatus::Active);
        EXPECT_EQ(stored.failedAttempts(), std::uint8_t{ 0 });
        EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 0 });
    }

    TEST(A1Auth, CorrectPinAfterFailedAttempts) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        const auto wrongPin = PinCode::parse("0000");
        const auto correctPin = PinCode::parse("1234");
        storage.seed(Card(clientCard, "Client", hasher.hash("1234"), CardKind::Client));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { return now; });

        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::WrongPin);
        EXPECT_EQ(storage.cards().find(clientCard).value().failedAttempts(), std::uint8_t{ 1 });

        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::WrongPin);
        EXPECT_EQ(storage.cards().find(clientCard).value().failedAttempts(), std::uint8_t{ 2 });
        EXPECT_EQ(storage.cards().find(clientCard).value().status(), CardStatus::Active);

        EXPECT_EQ(service.authenticate(clientCard, correctPin), AuthStatus::Ok);

        const Card stored = storage.cards().find(clientCard).value();
        EXPECT_EQ(stored.status(), CardStatus::Active);
        EXPECT_EQ(stored.failedAttempts(), std::uint8_t{ 0 });
        EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 0 });
    }

    TEST(A2Auth, ThreeFailedAttemptsRetainCardAndLogOnce) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        const auto wrongPin = PinCode::parse("0000");
        storage.seed(Card(clientCard, "Client", hasher.hash("1234"), CardKind::Client));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { return now; });

        // 2 errors, then exit / cancel
        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::WrongPin);
        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::WrongPin);
        EXPECT_EQ(storage.cards().find(clientCard).value().failedAttempts(), std::uint8_t{ 2 });
        EXPECT_EQ(storage.cards().find(clientCard).value().status(), CardStatus::Active);
        EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 0 });

        // Re-inserting or canceling before PIN entry keeps the counter
        EXPECT_EQ(service.inspect(clientCard), CardInspection::Client);
        EXPECT_EQ(storage.cards().find(clientCard).value().failedAttempts(), std::uint8_t{ 2 });

        // third error -> status Retained
        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::Retained);

        const Card stored = storage.cards().find(clientCard).value();
        EXPECT_EQ(stored.status(), CardStatus::Retained);
        EXPECT_EQ(stored.failedAttempts(), std::uint8_t{ 3 });

        const auto entries = storage.journal().last(20);
        ASSERT_EQ(entries.size(), std::size_t{ 1 });
        EXPECT_EQ(entries[0].kind, EntryKind::CardRetained);
        EXPECT_EQ(entries[0].actor, clientCard);
        EXPECT_EQ(entries[0].atUtc, atm::domain::toEpochSeconds(now));
    }

    TEST(A2Auth, RetainedCardCannotAuthenticate) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        const auto wrongPin = PinCode::parse("0000");
        const auto correctPin = PinCode::parse("1234");
        storage.seed(Card(clientCard, "Client", hasher.hash("1234"), CardKind::Client));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { return now; });

        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::WrongPin);
        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::WrongPin);
        EXPECT_EQ(service.authenticate(clientCard, wrongPin), AuthStatus::Retained);
        ASSERT_EQ(storage.journal().last(20).size(), std::size_t{ 1 });

        EXPECT_EQ(service.inspect(clientCard), CardInspection::Retained);
        EXPECT_EQ(service.authenticate(clientCard, correctPin), AuthStatus::Retained);
        EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 1 });
    }

    TEST(A3Auth, ServiceCardNeverRetainedOnWrongPins) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto operatorCard = CardNumber::parse("4000000000000001");
        const auto operatorPin = PinCode::parse("9999");
        const auto wrongPin = PinCode::parse("0000");
        storage.seed(Card(operatorCard, "Operator", hasher.hash("9999"), CardKind::Service));

        int clockCalls = 0;
        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { ++clockCalls; return now; });

        EXPECT_EQ(service.inspect(operatorCard), CardInspection::Service);
        EXPECT_EQ(service.authenticate(operatorCard, operatorPin), AuthStatus::Ok);
        EXPECT_EQ(storage.commits, 0);

        for (int attempt = 0; attempt < 4; ++attempt) {
            EXPECT_EQ(service.authenticate(operatorCard, wrongPin), AuthStatus::WrongPin);
        }

        const Card stored = storage.cards().find(operatorCard).value();
        EXPECT_EQ(stored.status(), CardStatus::Active);
        EXPECT_EQ(stored.failedAttempts(), std::uint8_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
        EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 0 });
        EXPECT_EQ(clockCalls, 0);

        EXPECT_EQ(service.authenticate(operatorCard, operatorPin), AuthStatus::Ok);
    }

    TEST(A3Auth, UnknownAndAlreadyRetainedCardsAreRejectedWithoutDatabaseChanges) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto retainedCard = CardNumber::parse("4000000000000003");
        const auto unknownCard = CardNumber::parse("9000000000009999");
        const auto pin = PinCode::parse("1234");
        storage.seed(Card(retainedCard, "Retained", hasher.hash("1234"), CardKind::Client, CardStatus::Retained, 3));

        int clockCalls = 0;
        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { ++clockCalls; return now; });

        EXPECT_EQ(service.inspect(unknownCard), CardInspection::Unknown);
        EXPECT_DOMAIN_ERROR((void)service.authenticate(unknownCard, pin), ErrorReason::UnknownCard);

        EXPECT_EQ(service.inspect(retainedCard), CardInspection::Retained);
        EXPECT_EQ(service.authenticate(retainedCard, pin), AuthStatus::Retained);

        EXPECT_EQ(storage.commits, 0);
        EXPECT_EQ(storage.journal().last(20).size(), std::size_t{ 0 });
        EXPECT_EQ(clockCalls, 0);
    }

    TEST(A3Auth, InvalidCardAndPinFormatsRejected) {
        EXPECT_THROW((void)CardNumber::parse(""), std::invalid_argument);
        EXPECT_THROW((void)CardNumber::parse("1234"), std::invalid_argument);
        EXPECT_THROW((void)CardNumber::parse("40000000000000010"), std::invalid_argument);
        EXPECT_THROW((void)CardNumber::parse("4000 0000 0000 0001"), std::invalid_argument);
        EXPECT_THROW((void)CardNumber::parse("400000000000000a"), std::invalid_argument);

        EXPECT_THROW((void)PinCode::parse(""), std::invalid_argument);
        EXPECT_THROW((void)PinCode::parse("123"), std::invalid_argument);
        EXPECT_THROW((void)PinCode::parse("12345"), std::invalid_argument);
        EXPECT_THROW((void)PinCode::parse("12a4"), std::invalid_argument);
        EXPECT_THROW((void)PinCode::parse("-123"), std::invalid_argument);
    }

    TEST(N1Auth, ChangesPinSuccessfully) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        const auto oldPin = PinCode::parse("1234");
        const auto newPin = PinCode::parse("5678");
        const std::string oldHash = hasher.hash("1234");
        storage.seed(Card(clientCard, "Client", oldHash, CardKind::Client));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { return now; });

        service.changePin(clientCard, oldPin, newPin);

        const Card updated = storage.cards().find(clientCard).value();
        EXPECT_FALSE(updated.pinHash() == oldHash);
        EXPECT_EQ(updated.failedAttempts(), std::uint8_t{ 0 });
        EXPECT_EQ(storage.commits, 1);

        const auto entries = storage.journal().last(20);
        ASSERT_EQ(entries.size(), std::size_t{ 1 });
        EXPECT_EQ(entries[0].kind, EntryKind::PinChange);
        EXPECT_EQ(entries[0].actor, clientCard);
        EXPECT_EQ(entries[0].atUtc, atm::domain::toEpochSeconds(now));

        EXPECT_EQ(service.authenticate(clientCard, newPin), AuthStatus::Ok);
        EXPECT_EQ(service.authenticate(clientCard, oldPin), AuthStatus::WrongPin);
    }

    TEST(N1Auth, RejectsWrongOldPin) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        const std::string oldHash = hasher.hash("1234");
        storage.seed(Card(clientCard, "Client", oldHash, CardKind::Client, CardStatus::Active, 1));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { return now; });

        EXPECT_DOMAIN_ERROR(service.changePin(clientCard, PinCode::parse("0000"), PinCode::parse("5678")), ErrorReason::IncorrectPin);

        const Card stored = storage.cards().find(clientCard).value();
        EXPECT_EQ(stored.pinHash(), oldHash);
        EXPECT_EQ(stored.failedAttempts(), std::uint8_t{ 1 });
        EXPECT_EQ(stored.status(), CardStatus::Active);
        EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
    }

    TEST(N1Auth, RejectsSameNewPin) {
        TestStorage storage;
        PinHasher hasher{ 10 };
        AtmConfig config;
        const auto clientCard = CardNumber::parse("4000000000000002");
        const std::string oldHash = hasher.hash("1234");
        storage.seed(Card(clientCard, "Client", oldHash, CardKind::Client, CardStatus::Active, 1));

        const auto now = atm::domain::fromEpochSeconds(1'768'471'200);
        AuthService service(storage, hasher, config, [&] { return now; });

        EXPECT_DOMAIN_ERROR(
            service.changePin(clientCard, PinCode::parse("1234"), PinCode::parse("1234")),
            ErrorReason::SamePin);

        const Card stored = storage.cards().find(clientCard).value();
        EXPECT_EQ(stored.pinHash(), oldHash);
        EXPECT_EQ(stored.failedAttempts(), std::uint8_t{ 1 });
        EXPECT_EQ(storage.journal().last(100).size(), std::size_t{ 0 });
        EXPECT_EQ(storage.commits, 0);
    }

}  // namespace
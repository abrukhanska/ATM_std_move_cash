#include "support/mini_test.hpp"
#include "domain/card.hpp"

#include <string>
#include <utility>

namespace {

using atm::domain::Card;
using atm::domain::CardKind;
using atm::domain::CardNumber;
using atm::domain::CardStatus;

static_assert(noexcept(std::declval<const Card&>().number()));
static_assert(noexcept(std::declval<const Card&>().kind()));
static_assert(noexcept(std::declval<const Card&>().status()));
static_assert(noexcept(std::declval<const Card&>().pinHash()));
static_assert(noexcept(std::declval<const Card&>().failedAttempts()));
static_assert(noexcept(std::declval<Card&>().changePinHash(std::string{})));
static_assert(noexcept(std::declval<Card&>().registerFailedAttempt()));
static_assert(noexcept(std::declval<Card&>().registerSuccess()));
static_assert(noexcept(std::declval<Card&>().release()));

// --- number & holderName & pinHash & kind & status & failedAttempts ---

TEST(V2Card, ConstructorInitializesFieldsCorrectly) {
    const auto number = CardNumber::parse("4000123456789010");
    const Card card(number, "Client", "hash123", CardKind::Client, CardStatus::Active, 1u);

    EXPECT_EQ(card.number(), number);
    EXPECT_EQ(card.holderName(), "Client");
    EXPECT_EQ(card.pinHash(), "hash123");
    EXPECT_EQ(card.kind(), CardKind::Client);
    EXPECT_EQ(card.status(), CardStatus::Active);
    EXPECT_EQ(card.failedAttempts(), 1u);

    const Card retainedCard(number, "Client", "hash123", CardKind::Client, CardStatus::Retained, 3u);
    EXPECT_EQ(retainedCard.status(), CardStatus::Retained);
    EXPECT_EQ(retainedCard.failedAttempts(), 3u);

    const auto serviceNumber = CardNumber::parse("4000123456789099");
    const Card serviceCard(serviceNumber, "Service", "service_hash", CardKind::Service);
    EXPECT_EQ(serviceCard.kind(), CardKind::Service);
    EXPECT_EQ(serviceCard.status(), CardStatus::Active);
    EXPECT_EQ(serviceCard.failedAttempts(), 0u);
}

TEST(V2Card, ConstructorRejectsInvalidStatusOrFailedAttempts) {
    const auto clientNumber = CardNumber::parse("4000123456789010");
    const auto serviceNumber = CardNumber::parse("4000123456789099");

    EXPECT_THROW((void)Card(clientNumber, "Client", "hash", CardKind::Client, CardStatus::Active, 3u), std::invalid_argument);
    EXPECT_THROW((void)Card(clientNumber, "Client", "hash", CardKind::Client, CardStatus::Active, 4u), std::invalid_argument);

    EXPECT_THROW((void)Card(clientNumber, "Client", "hash", CardKind::Client, CardStatus::Retained, 0u), std::invalid_argument);
    EXPECT_THROW((void)Card(clientNumber, "Client", "hash", CardKind::Client, CardStatus::Retained, 2u), std::invalid_argument);
    EXPECT_THROW((void)Card(clientNumber, "Client", "hash", CardKind::Client, CardStatus::Retained, 4u), std::invalid_argument);

    EXPECT_THROW((void)Card(serviceNumber, "Service", "hash", CardKind::Service, CardStatus::Retained, 0u), std::invalid_argument);
    EXPECT_THROW((void)Card(serviceNumber, "Service", "hash", CardKind::Service, CardStatus::Active, 1u), std::invalid_argument);
}

TEST(V2Card, FailedAttempts_RegisterFailedAttempts) {
    const auto number = CardNumber::parse("4000123456789010");
    Card card(number, "Client", "hash123", CardKind::Client);

    EXPECT_EQ(card.failedAttempts(), 0u);
    EXPECT_EQ(card.status(), CardStatus::Active);

    // first failed attempt
    card.registerFailedAttempt();
    EXPECT_EQ(card.failedAttempts(), 1u);
    EXPECT_EQ(card.status(), CardStatus::Active);

    // second failed attempt
    card.registerFailedAttempt();
    EXPECT_EQ(card.failedAttempts(), 2u);
    EXPECT_EQ(card.status(), CardStatus::Active);

    // third failed attempt -> card should be retained
    card.registerFailedAttempt();
    EXPECT_EQ(card.failedAttempts(), 3u);
    EXPECT_EQ(card.status(), CardStatus::Retained);

    // fourth failed attempt -> counter should not increase, card remains retained
    card.registerFailedAttempt();
    EXPECT_EQ(card.failedAttempts(), 3u);
    EXPECT_EQ(card.status(), CardStatus::Retained);
}

TEST(V2Card, Register_ResetsFailedAttempts) {
    const auto number = CardNumber::parse("4000123456789010");
    Card card(number, "Client", "hash123", CardKind::Client, CardStatus::Active, 2u);

    EXPECT_EQ(card.failedAttempts(), 2u);

    card.registerSuccess();
    EXPECT_EQ(card.failedAttempts(), 0u);
    EXPECT_EQ(card.status(), CardStatus::Active);
}

TEST(V2Card, ServiceCard_RegisterFailedAttempts_DoesNotChangeState) {
    const auto number = CardNumber::parse("4000123456789099");
    Card serviceCard(number, "Service", "service_hash", CardKind::Service);

    serviceCard.registerFailedAttempt();
    EXPECT_EQ(serviceCard.failedAttempts(), 0u);
    EXPECT_EQ(serviceCard.status(), CardStatus::Active);
}

TEST(V2Card, ClientCard_Release_SetsStatusActiveAndResetsFailedAttempts) {
    const auto number = CardNumber::parse("4000123456789010");
    Card card(number, "Client", "hash123", CardKind::Client, CardStatus::Retained, 3u);

    card.release();
    EXPECT_EQ(card.status(), CardStatus::Active);
    EXPECT_EQ(card.failedAttempts(), 0u);
}

TEST(V2Card, ChangePinHash_Success_UpdatesPinHash) {
    const auto number = CardNumber::parse("4000123456789010");
    Card card(number, "Client", "old_hash", CardKind::Client);

    card.changePinHash("new_hash");
    EXPECT_EQ(card.pinHash(), "new_hash");
}

} // namespace
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
static_assert(noexcept(std::declval<Card&>().registerFailedAttempts()));
static_assert(noexcept(std::declval<Card&>().registerSuccess()));
static_assert(noexcept(std::declval<Card&>().release()));

// --- number & pinHash & kind & status & failedAttempts ---

TEST(V2Card, ConstructorInitializesFieldsCorrectly) {
    const auto num = CardNumber::parse("4000123456789010");
    const Card card(num, "hash123", CardKind::Client, CardStatus::Active, 1u);

    EXPECT_EQ(card.number(), num);
    EXPECT_EQ(card.pinHash(), "hash123");
    EXPECT_EQ(card.kind(), CardKind::Client);
    EXPECT_EQ(card.status(), CardStatus::Active);
    EXPECT_EQ(card.failedAttempts(), 1);
}

TEST(V2Card, ServiceCard_ConstructorOverridesStatusAndFailedAttempts) {
    const auto num = CardNumber::parse("4000123456789099");
    const Card serviceCard(num, "service_hash", CardKind::Service, CardStatus::Retained, 2u);

    EXPECT_EQ(serviceCard.kind(), CardKind::Service);
    EXPECT_EQ(serviceCard.status(), CardStatus::Active);
    EXPECT_EQ(serviceCard.failedAttempts(), 0u);
}

TEST(V2Card, FailedAttempts_RegisterFailedAttempts) {
    const auto num = CardNumber::parse("4000123456789010");
    Card card(num, "hash123", CardKind::Client);

    EXPECT_EQ(card.failedAttempts(), 0u);
    EXPECT_EQ(card.status(), CardStatus::Active);

    // first failed attempt
    card.registerFailedAttempts();
    EXPECT_EQ(card.failedAttempts(), 1u);
    EXPECT_EQ(card.status(), CardStatus::Active);

    // second failed attempt
    card.registerFailedAttempts();
    EXPECT_EQ(card.failedAttempts(), 2u);
    EXPECT_EQ(card.status(), CardStatus::Active);

    // third failed attempt -> card should be retained
    card.registerFailedAttempts();
    EXPECT_EQ(card.failedAttempts(), 3u);
    EXPECT_EQ(card.status(), CardStatus::Retained);

    // fourth failed attempt -> counter should not increase, card remains retained
    card.registerFailedAttempts();
    EXPECT_EQ(card.failedAttempts(), 3u);
    EXPECT_EQ(card.status(), CardStatus::Retained);
}

TEST(V2Card, Register_ResetsFailedAttempts) {
    const auto num = CardNumber::parse("4000123456789010");
    Card card(num, "hash123", CardKind::Client, CardStatus::Active, 2u);

    EXPECT_EQ(card.failedAttempts(), 2u);

    card.registerSuccess();
    EXPECT_EQ(card.failedAttempts(), 0u);
    EXPECT_EQ(card.status(), CardStatus::Active);
}

TEST(V2Card, ServiceCard_RegisterFailedAttempts_DoesNotChangeState) {
    const auto num = CardNumber::parse("4000123456789099");
    Card serviceCard(num, "service_hash", CardKind::Service);

    serviceCard.registerFailedAttempts();
    EXPECT_EQ(serviceCard.failedAttempts(), 0u);
    EXPECT_EQ(serviceCard.status(), CardStatus::Active);
}

TEST(V2Card, ClientCard_Release_SetsStatusActiveAndResetsFailedAttempts) {
    const auto num = CardNumber::parse("4000123456789010");
    Card card(num, "hash123", CardKind::Client, CardStatus::Retained, 3u);

    card.release();
    EXPECT_EQ(card.status(), CardStatus::Active);
    EXPECT_EQ(card.failedAttempts(), 0u);
}

TEST(V2Card, ChangePinHash_Success_UpdatesPinHash) {
    const auto num = CardNumber::parse("4000123456789010");
    Card card(num, "old_hash", CardKind::Client);

    card.changePinHash("new_hash_456");
    EXPECT_EQ(card.pinHash(), "new_hash_456");
}

} // namespace
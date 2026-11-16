#include "presentation/atm_state.hpp"
#include "presentation/session.hpp"
#include "presentation/support_channel.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/user_interface.hpp"

#include "domain/istorage.hpp"
#include "domain/card.hpp"
#include "domain/journal_entry.hpp"
#include "application/device_service.hpp"
#include "domain/money.hpp"
#include "domain/pin_code.hpp"
#include "presentation/atm_machine.hpp"
#include "presentation/atm_context.hpp"
#include "domain/transaction.hpp"
#include "infrastructure/sql/database.hpp"

#include <string>
#include <type_traits>
#include <utility>

namespace {

    using namespace atm::presentation;
    using namespace atm::domain;

    static_assert(std::has_virtual_destructor_v<AtmState>);
    static_assert(std::has_virtual_destructor_v<IUserInterface>);
    static_assert(std::has_virtual_destructor_v<IReceiptPrinter>);
    static_assert(std::has_virtual_destructor_v<ISupportChannel>);
    static_assert(std::has_virtual_destructor_v<IStorage>);

    static_assert(!std::is_copy_constructible_v<Session>);
    static_assert(!std::is_copy_assignable_v<Session>);
    static_assert(std::is_nothrow_move_constructible_v<Session>);
    static_assert(std::is_nothrow_move_assignable_v<Session>);

    static_assert(std::is_nothrow_move_constructible_v<JournalEntry>);

    static_assert(noexcept(std::declval<AtmState&>().~AtmState()));
    static_assert(noexcept(std::declval<IStorage&>().rollback()));
    static_assert(noexcept(std::declval<IStorage&>().broken()));

    static_assert(noexcept(
        std::declval<Card&>().changePinHash(
            std::declval<std::string>()
        )
        ));

    static_assert(noexcept(
        std::declval<const atm::application::DeviceService&>().isInService()
        ));

    static_assert(noexcept(
        std::declval<const atm::application::DeviceService&>().isBroken()
        ));

    static_assert(noexcept(
        std::declval<const Money&>().kopiykas()
        ));

    static_assert(noexcept(
        std::declval<const PinCode&>().digits()
        ));


    static_assert(noexcept(std::declval<const Card&>().number()));
    static_assert(noexcept(std::declval<const Card&>().holderName()));
    static_assert(noexcept(std::declval<const Card&>().pinHash()));
    static_assert(noexcept(std::declval<const Card&>().kind()));
    static_assert(noexcept(std::declval<const Card&>().status()));
    static_assert(noexcept(std::declval<const Card&>().failedAttempts()));

    static_assert(noexcept(std::declval<Card&>().registerFailedAttempt()));
    static_assert(noexcept(std::declval<Card&>().registerSuccess()));
    static_assert(noexcept(std::declval<Card&>().release()));


    static_assert(!std::is_copy_constructible_v<AtmMachine>);
    static_assert(!std::is_copy_assignable_v<AtmMachine>);
    static_assert(!std::is_move_constructible_v<AtmMachine>);
    static_assert(!std::is_move_assignable_v<AtmMachine>);


    static_assert(!std::is_copy_constructible_v<AtmContext>);
    static_assert(!std::is_copy_assignable_v<AtmContext>);
    static_assert(!std::is_move_constructible_v<AtmContext>);
    static_assert(!std::is_move_assignable_v<AtmContext>);




    static_assert(!std::is_copy_constructible_v<Transaction>);
    static_assert(!std::is_copy_assignable_v<Transaction>);
    static_assert(!std::is_move_constructible_v<Transaction>);
    static_assert(!std::is_move_assignable_v<Transaction>);

    static_assert(noexcept(std::declval<Transaction&>().~Transaction()));


    using atm::infrastructure::sql::Database;

    static_assert(!std::is_copy_constructible_v<Database>);
    static_assert(!std::is_copy_assignable_v<Database>);
    static_assert(!std::is_move_constructible_v<Database>);
    static_assert(!std::is_move_assignable_v<Database>);

    static_assert(noexcept(std::declval<Database&>().~Database()));




    static_assert(std::has_virtual_destructor_v<ICardRepository>);
    static_assert(std::has_virtual_destructor_v<IAccountRepository>);
    static_assert(std::has_virtual_destructor_v<ICashRepository>);
    static_assert(std::has_virtual_destructor_v<IJournalRepository>);

    static_assert(std::is_nothrow_destructible_v<ICardRepository>);
    static_assert(std::is_nothrow_destructible_v<IAccountRepository>);
    static_assert(std::is_nothrow_destructible_v<ICashRepository>);
    static_assert(std::is_nothrow_destructible_v<IJournalRepository>);


} // namespace


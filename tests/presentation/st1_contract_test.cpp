#include "presentation/atm_state.hpp"
#include "presentation/session.hpp"
#include "presentation/support_channel.hpp"
#include "presentation/receipt_printer.hpp"
#include "presentation/user_interface.hpp"

#include <type_traits>
#include <utility>

namespace {

    using namespace atm::presentation;

    static_assert(std::has_virtual_destructor_v<AtmState>);
    static_assert(std::has_virtual_destructor_v<IUserInterface>);
    static_assert(std::has_virtual_destructor_v<IReceiptPrinter>);
    static_assert(std::has_virtual_destructor_v<ISupportChannel>);

    static_assert(!std::is_copy_constructible_v<Session>);
    static_assert(!std::is_copy_assignable_v<Session>);
    static_assert(std::is_nothrow_move_constructible_v<Session>);

    static_assert(
        noexcept(std::declval<AtmState&>().~AtmState())
        );

} // namespace

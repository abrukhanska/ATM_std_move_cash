#ifndef ATM_APPLICATION_APP_TYPES_HPP
#define ATM_APPLICATION_APP_TYPES_HPP

namespace atm::application {

    enum class CardInspection { Unknown, Retained, Client, Service };
    enum class AuthStatus { Ok, WrongPin, Retained };

} //namespace atm::application

#endif // ATM_APPLICATION_APP_TYPES_HPP
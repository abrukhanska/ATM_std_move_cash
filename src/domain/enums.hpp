#ifndef ATM_DOMAIN_ENUMS_HPP
#define ATM_DOMAIN_ENUMS_HPP

#include <cstdint>

namespace atm::domain {

    enum class CardKind { Client, Service };
    enum class CardStatus { Active, Retained };
    enum class Denomination { UAH100, UAH200, UAH500, UAH1000, UAH2000 };
    enum class PayeeCategory { MobileOperator, Charity };
    
    // Додай інші перелічення зі специфікації
}

#endif // ATM_DOMAIN_ENUMS_HPP

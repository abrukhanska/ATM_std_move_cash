#include "pin_hasher.hpp"

std::string atm::application::PinHasher::hash(std::string_view pin) const
{
    return std::string();
}

bool atm::application::PinHasher::matches(std::string_view pin, std::string_view hash) const
{
    return false;
}
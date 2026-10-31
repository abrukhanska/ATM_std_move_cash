#include "domain/money.hpp"

#include <string>

namespace atm::domain {
	std::string Money::format() const {
		const auto whole = kopiykas_ / kKopiykasPerUah;	
		const auto fraction = kopiykas_ % kKopiykasPerUah;

		std::string result = std::to_string(whole);
		result += '.';
		result += static_cast<char>('0' + fraction / 10);
		result += static_cast<char>('0' + fraction % 10);
		return result;
	}
}
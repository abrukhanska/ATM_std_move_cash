#include "application/pin_hasher.hpp"
#include "domain/domain_error.hpp"

#include <array>
#include "picosha2.h"
#include <random>
#include <charconv>

namespace atm::application {

    std::string PinHasher::hash(std::string_view pin) const
    {
        std::random_device rd;
        std::array<unsigned char, 16> salt{};
        for (auto& byte : salt) {
            byte = static_cast<unsigned char>(rd());
        }

        std::vector<unsigned char> bytes;
        bytes.reserve(salt.size() + pin.size());
        bytes.insert(bytes.end(), salt.begin(), salt.end());
        for (char ch : pin) {
            bytes.push_back(static_cast<unsigned char>(ch));
        }

        std::array<unsigned char, picosha2::k_digest_size> digest{};
        picosha2::hash256(bytes.begin(), bytes.end(), digest.begin(), digest.end());

        for (std::uint32_t i = 1; i < iterations_; ++i) {
            picosha2::hash256(digest.begin(), digest.end(), digest.begin(), digest.end());
        }

        const std::string saltHex = picosha2::bytes_to_hex_string(salt.begin(), salt.end());
        const std::string hashHex = picosha2::bytes_to_hex_string(digest.begin(), digest.end());

        return "sha256$" + std::to_string(iterations_) + "$" + saltHex + "$" + hashHex;
    }

    bool PinHasher::matches(std::string_view pin, std::string_view hash) const
    {
        const std::size_t pos1 = hash.find('$');
        if (pos1 == std::string_view::npos) throw domain::DomainError(domain::ErrorReason::InvalidData);
        const std::size_t pos2 = hash.find('$', pos1 + 1);
        if (pos2 == std::string_view::npos) throw domain::DomainError(domain::ErrorReason::InvalidData);
        const std::size_t pos3 = hash.find('$', pos2 + 1);
        if (pos3 == std::string_view::npos) throw domain::DomainError(domain::ErrorReason::InvalidData);
        if (hash.find('$', pos3 + 1) != std::string_view::npos) {
            throw domain::DomainError(domain::ErrorReason::InvalidData);
        }

        const std::string_view algorithm = hash.substr(0, pos1);
        const std::string_view parameters = hash.substr(pos1 + 1, pos2 - pos1 - 1);
        const std::string_view saltSV = hash.substr(pos2 + 1, pos3 - pos2 - 1);
        const std::string_view expectedHex = hash.substr(pos3 + 1);

        if (algorithm != "sha256" || saltSV.size() != 32 || expectedHex.size() != 64) {
             throw domain::DomainError(domain::ErrorReason::InvalidData);
        }

        std::uint32_t iterations = 0;
        const auto [iterationsEnd, iterationsErrc] = std::from_chars(parameters.data(), parameters.data() + parameters.size(), iterations);

        if (iterationsErrc != std::errc{} || iterationsEnd != parameters.data() + parameters.size() || iterations == 0) {
            throw domain::DomainError(domain::ErrorReason::InvalidData);
        }

        std::array<unsigned char, 16> salt{};
        for (std::size_t i = 0; i < salt.size(); ++i) {
            unsigned int byte = 0;
            const char* shift = saltSV.data() + i * 2;
            const auto [hexEnd, hexErrc] = std::from_chars(shift, shift + 2, byte, 16);

            if (hexErrc != std::errc{} || hexEnd != shift + 2) {
                throw domain::DomainError(domain::ErrorReason::InvalidData);
            }

            salt[i] = static_cast<unsigned char>(byte);
        }

        for (char ch : expectedHex) {
            const bool isHex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
            if (!isHex) {
                throw domain::DomainError(domain::ErrorReason::InvalidData);
            }
        }

        std::vector<unsigned char> bytes;
        bytes.reserve(salt.size() + pin.size());
        bytes.insert(bytes.end(), salt.begin(), salt.end());
        for (char ch : pin) {
               bytes.push_back(static_cast<unsigned char>(ch));
        }

        std::array<unsigned char, picosha2::k_digest_size> digest{};
        picosha2::hash256(bytes.begin(), bytes.end(), digest.begin(), digest.end());

        for (std::uint32_t i = 1; i < iterations; ++i) {
            picosha2::hash256(digest.begin(), digest.end(), digest.begin(), digest.end());
        }

        const std::string hex = picosha2::bytes_to_hex_string(digest.begin(), digest.end());

        unsigned char diff = 0;
        for (std::size_t i = 0; i < expectedHex.size(); ++i) {
            diff |= static_cast<unsigned char>(hex[i]) ^ static_cast<unsigned char>(expectedHex[i]);
        }
        return diff == 0;
    }

} //namespace atm::application

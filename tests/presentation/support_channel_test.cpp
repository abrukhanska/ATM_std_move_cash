#include "support/mini_test.hpp"
#include "presentation/support_channel.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

    using atm::presentation::LoggingSupportChannel;

    namespace fs = std::filesystem;

    const fs::path testFile = "test_support_channel.log";

    std::string readLog() {
        std::ifstream file(testFile);
        return std::string(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
    }

    TEST(U1SupportChannel, WritesMessage) {
        fs::remove(testFile);

        {
            LoggingSupportChannel channel(testFile.string());
            channel.send("Need assistance");
        }

        EXPECT_EQ(readLog(), std::string("Need assistance\n"));

        fs::remove(testFile);
    }

    TEST(U1SupportChannel, AppendsMessages) {
        fs::remove(testFile);

        {
            LoggingSupportChannel channel(testFile.string());
            channel.send("First message");
        }

        {
            LoggingSupportChannel channel(testFile.string());
            channel.send("Second message");
        }

        EXPECT_EQ(
            readLog(),
            std::string("First message\nSecond message\n")
        );

        fs::remove(testFile);
    }

    TEST(U1SupportChannel, InvalidPathThrows) {
        const fs::path missingDir =
            "nonexistent_support_test_directory";

        fs::remove_all(missingDir);

        EXPECT_THROW(
            LoggingSupportChannel(
                (missingDir / "support.log").string()
            ),
            std::runtime_error
        );
    }

    TEST(U1SupportChannel, InvalidPathDoesNotCreateLog) {
        const fs::path missingDir =
            "nonexistent_support_test_directory";

        fs::remove_all(missingDir);

        try {
            LoggingSupportChannel channel(
                (missingDir / "support.log").string()
            );
        }
        catch (const std::runtime_error&) {
        }

        EXPECT_FALSE(
            fs::exists(missingDir / "support.log")
        );
    }

} // namespace

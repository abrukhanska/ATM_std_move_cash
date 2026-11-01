#ifndef ATM_TESTS_SUPPORT_MINI_TEST_HPP
#define ATM_TESTS_SUPPORT_MINI_TEST_HPP

// Minimal in-house test framework (instead of GoogleTest).
// Syntax mirrors GoogleTest: TEST(Suite, Name) { EXPECT_EQ(a, b); }
// Supported: TEST, EXPECT_TRUE/FALSE/EQ/NE, EXPECT_THROW, EXPECT_NO_THROW,
// ASSERT_TRUE/ASSERT_EQ (leave the test on failure), FAIL() << "text".
// Unlike GoogleTest, FAIL() records a failure but does NOT leave the test.
// Exit code of the runner: 0 = all passed, 1 = failures, 2 = no test matched.

#include <concepts>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mini_test {

    struct TestCase {
        std::string name;
        std::function<void()> body;
    };

    inline std::vector<TestCase>& registry() {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline int& failureCount() {
        static int count = 0;
        return count;
    }

    struct Registrar {
        Registrar(std::string name, std::function<void()> body) {
            registry().push_back(TestCase{ std::move(name), std::move(body) });
        }
    };

    // Text for failure messages: operator<<, then .format() (Money), then the
    // numeric value of an enum, otherwise a placeholder.
    template <typename T>
    std::string toText([[maybe_unused]] const T& value) {
        if constexpr (requires(std::ostringstream & out) { out << value; }) {
            std::ostringstream out;
            out << value;
            return out.str();
        }
        else if constexpr (requires {
            { value.format() } -> std::convertible_to<std::string>;
        }) {
            return value.format();
        }
        else if constexpr (std::is_enum_v<T>) {
            return std::to_string(
                static_cast<long long>(static_cast<std::underlying_type_t<T>>(value)));
        }
        else {
            return "<value>";
        }
    }

    // Temporary object: collects text through << and counts a failure when destroyed.
    class Failure {
    public:
        Failure(const char* file, int line) : file_(file), line_(line) {}
        Failure(const Failure&) = delete;
        Failure& operator=(const Failure&) = delete;
        ~Failure() {
            ++failureCount();
            std::cerr << "  FAILED " << file_ << ":" << line_ << "  " << text_.str() << "\n";
        }
        template <typename T>
        Failure& operator<<(const T& value) {
            text_ << value;
            return *this;
        }

    private:
        const char* file_;
        int line_;
        std::ostringstream text_;
    };

    template <typename A, typename B>
    bool expectEqual(const A& a, const B& b, const char* ea, const char* eb, const char* file,
        int line) {
        if (a == b) {
            return true;
        }
        Failure(file, line) << ea << " == " << eb << "   [" << toText(a) << " vs " << toText(b)
            << "]";
        return false;
    }

    template <typename A, typename B>
    bool expectNotEqual(const A& a, const B& b, const char* ea, const char* eb, const char* file,
        int line) {
        if (a != b) {
            return true;
        }
        Failure(file, line) << ea << " != " << eb << "   [both " << toText(a) << "]";
        return false;
    }

}  // namespace mini_test

#define TEST(suite, name)                                                                 \
    static void suite##_##name##_body();                                                  \
    static const ::mini_test::Registrar suite##_##name##_registrar(#suite "." #name,     \
                                                                   suite##_##name##_body); \
    static void suite##_##name##_body()

#define FAIL() ::mini_test::Failure(__FILE__, __LINE__)

#define EXPECT_TRUE(cond)                                               \
    do {                                                                \
        if (!(cond)) ::mini_test::Failure(__FILE__, __LINE__) << #cond; \
    } while (0)

#define EXPECT_FALSE(cond)                                                    \
    do {                                                                      \
        if (cond) ::mini_test::Failure(__FILE__, __LINE__) << "!(" #cond ")"; \
    } while (0)

#define ASSERT_TRUE(cond)                                                    \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ::mini_test::Failure(__FILE__, __LINE__) << #cond;               \
            return;                                                          \
        }                                                                    \
    } while (0)

#define EXPECT_EQ(a, b) ((void)::mini_test::expectEqual((a), (b), #a, #b, __FILE__, __LINE__))
#define EXPECT_NE(a, b) \
    ((void)::mini_test::expectNotEqual((a), (b), #a, #b, __FILE__, __LINE__))

#define ASSERT_EQ(a, b)                                                              \
    do {                                                                             \
        if (!::mini_test::expectEqual((a), (b), #a, #b, __FILE__, __LINE__)) return; \
    } while (0)

#define EXPECT_NO_THROW(stmt)                                                               \
    do {                                                                                    \
        try {                                                                               \
            stmt;                                                                           \
        } catch (const std::exception& e_) {                                                \
            ::mini_test::Failure(__FILE__, __LINE__)                                        \
                << "unexpected exception in " #stmt ": " << e_.what();                      \
        } catch (...) {                                                                     \
            ::mini_test::Failure(__FILE__, __LINE__) << "unexpected exception in " #stmt;   \
        }                                                                                   \
    } while (0)

#define EXPECT_THROW(stmt, ExceptionType)                                                      \
    do {                                                                                       \
        bool thrown_ = false;                                                                  \
        bool wrongType_ = false;                                                               \
        try {                                                                                  \
            stmt;                                                                              \
        } catch (const ExceptionType&) {                                                       \
            thrown_ = true;                                                                    \
        } catch (...) {                                                                        \
            wrongType_ = true;                                                                 \
        }                                                                                      \
        if (wrongType_)                                                                        \
            ::mini_test::Failure(__FILE__, __LINE__)                                           \
                << "wrong exception type, expected " #ExceptionType " in " #stmt;              \
        else if (!thrown_)                                                                     \
            ::mini_test::Failure(__FILE__, __LINE__)                                           \
                << "no exception, expected " #ExceptionType " in " #stmt;                      \
    } while (0)

#endif  // ATM_TESTS_SUPPORT_MINI_TEST_HPP
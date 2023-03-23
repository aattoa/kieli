#pragma once

#include "utl/utilities.hpp"
#include "utl/color.hpp"


namespace tests {

    auto run_all_tests() -> void;


    struct Failure : utl::Exception {
        std::source_location thrower;

        explicit Failure(std::string&& message,
                         std::source_location = std::source_location::current());
    };
    

    struct Test {
        enum class Type {
            normal,   // Non-throwing test
            failing,  // Test that should throw test::Failure
            throwing, // Test that should throw an exception derived from std::exception, but not test::Failure
        };

        std::string_view name;
        Type             type;

        struct Invoke {
            std::function<void()> callable;
            std::source_location  caller;

            Invoke(auto&& callable, std::source_location const caller = std::source_location::current())
                requires (!std::same_as<Invoke, std::remove_cvref_t<decltype(callable)>>)
                : callable { std::forward<decltype(callable)>(callable) }
                , caller   { caller } {}
        };

        auto operator=(Invoke&&) -> void;
    };


    auto assert_eq(auto const& a,
                   auto const& b,
                   std::source_location caller = std::source_location::current())
    {
        if (a != b) {
            throw Failure {
                fmt::format(
                    "{}{}{} != {}{}{}",
                    utl::Color::red,
                    a,
                    utl::Color::white,
                    utl::Color::red,
                    b,
                    utl::Color::white
                ),
                caller
            };
        }
    }


    inline auto operator"" _test(char const* const s, utl::Usize const n) noexcept -> Test {
        return { .name { s, n }, .type = Test::Type::normal };
    }

    inline auto operator"" _failing_test(char const* const s, utl::Usize const n) noexcept -> Test {
        return { .name { s, n }, .type = Test::Type::failing };
    }

    inline auto operator"" _throwing_test(char const* const s, utl::Usize const n) noexcept -> Test {
        return { .name { s, n }, .type = Test::Type::throwing };
    }


    namespace dtl {
        struct Test_adder {
            explicit Test_adder(void(*)());
        };
    }

}


#define REGISTER_TEST_IMPL_IMPL(test_function, line) \
static_assert(std::same_as<void(), decltype(test_function)>, \
    "The given test function (" #test_function ", registered on line " #line ") isn't of type `void()`"); \
namespace { namespace test_impl { static tests::dtl::Test_adder test_adder_ ## line { &test_function }; } }

#define REGISTER_TEST_IMPL(test_function, line) REGISTER_TEST_IMPL_IMPL(test_function, line)
#define REGISTER_TEST(test_function) REGISTER_TEST_IMPL(test_function, __LINE__)
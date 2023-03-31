#include "utl/utilities.hpp"
#include "utl/color.hpp"
#include "utl/timer.hpp"
#include "tests.hpp"


namespace {

    constinit utl::Usize success_count = 0;
    constinit utl::Usize test_count    = 0;

    auto red_note() -> std::string_view {
        static auto const note = fmt::format("{}NOTE:{}", utl::Color::red, utl::Color::white);
        return note;
    }

    auto test_vector() noexcept -> std::vector<void(*)()>& { // Avoids SIOF
        static std::vector<void(*)()> vector;
        return vector;
    }

}


tests::Failure::Failure(std::string&& message, std::source_location const thrower)
    : Exception { std::move(message) }
    , thrower   { thrower } {}


auto tests::Test::operator=(Invoke&& test) -> void {
    auto const test_name = [&] {
        return fmt::format("[{}.{}]", test.caller.function_name(), this->name);
    };

    ++test_count;

    try {
        test.callable();
    }
    catch (Failure const& failure) {
        if (type != Type::failing) {
            fmt::print(
                "{} Test case failed in {}, on line {}: {}\n",
                red_note(),
                test_name(),
                failure.thrower.line(),
                failure.what());
        }
        else {
            ++success_count;
        }
        return;
    }
    catch (std::exception const& exception) {
        if (type != Type::throwing) {
            fmt::print(
                "{} Exception thrown during test {}: {}\n",
                red_note(),
                test_name(),
                exception.what());
        }
        else {
            ++success_count;
        }
        return;
    }
    catch (...) {
        fmt::print(
            "{} Unknown exception thrown during test {}\n",
            red_note(),
            test_name());
        throw; // Not the test's responsibility at this point
    }

    switch (type) {
    case Type::normal:
    {
        ++success_count;
        return;
    }
    case Type::failing:
    {
        fmt::print(
            "{} Test {} should have failed, but didn't\n",
            red_note(),
            test_name());
        return;
    }
    case Type::throwing:
    {
        fmt::print(
            "{} Test {} should have thrown an exception, but didn't\n",
            red_note(),
            test_name());
        return;
    }
    default:
        utl::unreachable();
    }
}


tests::dtl::Test_adder::Test_adder(void(* const test)()) {
    test_vector().push_back(test);
}


auto tests::run_all_tests() -> void {
    utl::Timer const test_timer;

    for (auto const test : test_vector())
        test();

    if (success_count == test_count) {
        fmt::print(
            "{}All {} tests passed! ({}){}\n",
            utl::Color::green,
            test_count,
            test_timer.elapsed(),
            utl::Color::white);
    }
}
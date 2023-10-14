#include <libutl/common/utilities.hpp>
#include <libutl/diag/diag.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE("libutl-diag interface " name, "[libutl][diag]")

TEST("diagnostic formatting")
{
    utl::diag::Context          context;
    utl::diag::Diagnostic const diagnostic {
        .message   = context.format_relative("qwerty {}", "asdf"),
        .help_note = context.format_relative("hello"),
        .level     = utl::diag::Level::warning,
    };

    auto const colors = utl::diag::Colors::defaults();
    REQUIRE(
        context.format_diagnostic(diagnostic, colors)
        == std::format("{}Warning:{} qwerty asdf\n\nhello", colors.warning, colors.normal));
}

#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libutl/common/timer.hpp>
#include <libutl/readline/readline.hpp>
#include <libutl/color/color.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libparse/parser_internals.hpp>
#include <libdesugar/desugar.hpp>
#include <libformat/format.hpp>
#include <cppargs.hpp>
#include <cppdiag.hpp>

namespace {

    [[nodiscard]] auto error_stream() -> std::ostream&
    {
        return std::cerr << utl::Color::red << "Error: " << utl::Color::white;
    }

    template <void (*f)(kieli::Lex_result&&)>
    auto generic_repl()
    {
        for (;;) {
            std::string string = utl::readline(">>> ");

            if (string.empty()) {
                continue;
            }
            else if (string == "q") {
                break;
            }

            utl::add_to_readline_history(string);

            try {
                kieli::Compilation_info repl_info
                    = kieli::mock_compilation_info(utl::diagnostics::Level::normal);
                utl::wrapper auto const repl_source
                    = repl_info.get()->source_arena.wrap("[repl]", std::move(string));
                f(kieli::lex({ .compilation_info = std::move(repl_info), .source = repl_source }));
            }
            catch (utl::diagnostics::Error const& error) {
                std::cerr << error.what() << "\n\n";
            }
            catch (std::exception const& exception) {
                error_stream() << exception.what() << "\n\n";
            }
        }
    }

    constexpr auto lexer_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        utl::print("Tokens: {}\n", lex_result.tokens);
    }>;

    constexpr auto expression_parser_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        libparse::Parse_context context { std::move(lex_result),
                                          cst::Node_arena::with_default_page_size() };
        if (auto result = parse_expression(context)) {
            utl::print("Result: {}\n", kieli::format_expression(**result, {}));
            if (!context.pointer->source_view.string.empty()) {
                utl::print("Remaining input: '{}'\n", context.pointer->source_view.string.data());
            }
        }
        else {
            utl::print("No parse\n");
        }
    }>;

    constexpr auto program_parser_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        auto parse_result = parse(std::move(lex_result));
        utl::print("{}", kieli::format_module(parse_result.module, {}));
    }>;

    constexpr auto desugaring_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        auto        desugar_result = desugar(parse(std::move(lex_result)));
        std::string output;
        for (ast::Definition const& definition : desugar_result.module.definitions) {
            ast::format_to(definition, output);
        }
        utl::print("{}\n\n", output);
    }>;

} // namespace

auto main(int argc, char const** argv) -> int
try {
    cppargs::Parameters parameters;

    auto const help_flag    = parameters.add('h', "help", "Show this help text");
    auto const version_flag = parameters.add('v', "version", "Show Kieli version");
    auto const nocolor_flag = parameters.add("nocolor", "Disable colored output");
    auto const time_flag    = parameters.add("time", "Print the execution time");
    auto const repl_option  = parameters.add<std::string_view>("repl", "Run the given REPL");

    cppargs::Arguments arguments = cppargs::parse(argc, argv, parameters);

    utl::Logging_timer const execution_timer { [&](auto const elapsed) {
        if (arguments[time_flag]) {
            utl::print("Total execution time: {}\n", elapsed);
        }
    } };

    if (arguments[nocolor_flag]) {
        utl::set_color_formatting_state(false);
    }

    if (arguments[help_flag]) {
        utl::print("Valid options:\n{}", parameters.help_string());
        return EXIT_SUCCESS;
    }

    if (arguments[version_flag]) {
        utl::print("kieli version 0, compiled on " __DATE__ ", " __TIME__ ".\n");
    }

    if (auto const name = arguments[repl_option]) {
        utl::Flatmap<std::string_view, void (*)()> const repls { {
            { "lex", lexer_repl },
            { "expr", expression_parser_repl },
            { "prog", program_parser_repl },
            { "des", desugaring_repl },
        } };
        if (auto const* const repl = repls.find(*name)) {
            (*repl)();
        }
        else {
            throw utl::exception("The repl must be one of lex|expr|prog|des|res, not '{}'", *name);
        }
    }

    return EXIT_SUCCESS;
}

catch (cppargs::Exception const& exception) {
    auto const&               info = exception.info();
    cppdiag::Context          context;
    cppdiag::Diagnostic const diagnostic {
        .text_sections { {
            .source_string  = info.command_line,
            .source_name    = "command line",
            .start_position = { .column = info.error_column },
            .stop_position  = { .column = -1 + info.error_column + info.error_width },
            .note           = context.message(cppargs::Parse_error_info::kind_to_string(info.kind)),
        } },
        .message   = context.message("Command line parse failure"),
        .help_note = context.format_message(
            "To see a list of valid options, use `{} --help`", *argv ? *argv : "kieli"),
        .level = cppdiag::Level::error,
    };
    std::cerr << context.format_diagnostic(diagnostic);
    return EXIT_FAILURE;
}
catch (utl::diagnostics::Error const& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
}
catch (std::exception const& exception) {
    error_stream() << exception.what() << '\n';
    return EXIT_FAILURE;
}
catch (...) {
    error_stream() << "Caught unrecognized exception\n";
    throw;
}

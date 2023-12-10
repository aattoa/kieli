#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libutl/common/timer.hpp>
#include <libutl/readline/readline.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libformat/format.hpp>
#include <cppargs.hpp>
#include <cppdiag.hpp>

namespace {

    [[nodiscard]] auto error_header(cppdiag::Colors const colors) -> std::string
    {
        return std::format("{}Error:{}", colors.error.code, colors.normal.code);
    }

    template <void (*f)(utl::Source::Wrapper, kieli::Compile_info&)>
    auto generic_repl(cppdiag::Colors const colors) -> void
    {
        for (;;) {
            auto input = utl::readline(">>> ");

            if (!input || input == "q") {
                break;
            }
            if (input.value().find_first_not_of(' ') == std::string::npos) {
                continue;
            }

            utl::add_to_readline_history(input.value());

            auto [info, source] = kieli::test_info_and_source(std::move(input.value()));
            try {
                f(source, info);
            }
            catch (kieli::Compilation_failure const&) {
                (void)0; // do nothing
            }
            catch (std::exception const& exception) {
                std::print(stderr, "{} {}\n\n", error_header(colors), exception.what());
            }
            std::print(stderr, "{}", info.diagnostics.format_all(colors));
        }
    }

    constexpr auto lex_repl
        = generic_repl<[](utl::Source::Wrapper const source, kieli::Compile_info& info) {
              std::print("Tokens: {}\n", kieli::lex(source, info));
          }>;

    constexpr auto parse_repl
        = generic_repl<[](utl::Source::Wrapper const source, kieli::Compile_info& info) {
              auto const tokens = kieli::lex(source, info);
              auto const module = kieli::parse(tokens, info);
              std::print("{}", kieli::format_module(module, {}));
          }>;

    constexpr auto desugar_repl
        = generic_repl<[](utl::Source::Wrapper const source, kieli::Compile_info& info) {
              auto const tokens = kieli::lex(source, info);
              auto const module = kieli::desugar(kieli::parse(tokens, info), info);

              std::string output;
              for (ast::Definition const& definition : module.definitions) {
                  ast::format_to(definition, output);
              }
              std::print("{}\n\n", output);
          }>;

} // namespace

auto main(int argc, char const** argv) -> int
{
    cppargs::Parameters parameters;

    auto const help_flag    = parameters.add('h', "help", "Show this help text");
    auto const version_flag = parameters.add('v', "version", "Show Kieli version");
    auto const nocolor_flag = parameters.add("nocolor", "Disable colored output");
    auto const time_flag    = parameters.add("time", "Print the execution time");
    auto const repl_option  = parameters.add<std::string_view>("repl", "Run the given REPL");

    cppdiag::Colors colors = cppdiag::Colors::defaults();

    try {
        cppargs::parse(argc, argv, parameters);

        utl::Logging_timer const execution_timer { [&](auto const elapsed) {
            if (time_flag) {
                std::print("Total execution time: {}\n", elapsed);
            }
        } };

        if (nocolor_flag) {
            colors = cppdiag::Colors {};
        }

        if (help_flag) {
            std::print("Valid options:\n{}", parameters.help_string());
            return EXIT_SUCCESS;
        }

        if (version_flag) {
            std::print("kieli version 0, compiled on " __DATE__ ", " __TIME__ ".\n");
        }

        if (repl_option) {
            utl::Flatmap<std::string_view, void (*)(cppdiag::Colors)> const repls { {
                { "lex", lex_repl },
                { "par", parse_repl },
                { "des", desugar_repl },
            } };
            if (auto const* const repl = repls.find(repl_option.value())) {
                (*repl)(colors);
            }
            else {
                throw utl::exception(
                    "The repl must be one of lex|par|des, not '{}'", repl_option.value());
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
                .note = context.message(cppargs::Parse_error_info::kind_to_string(info.kind)),
            } },
            .message   = context.message("Command line parse failure"),
            .help_note = context.format_message(
                "To see a list of valid options, use `{} --help`", *argv ? *argv : "kieli"),
            .severity = cppdiag::Severity::error,
        };
        std::print(stderr, "{}", context.format_diagnostic(diagnostic, colors));
        return EXIT_FAILURE;
    }
    catch (std::exception const& exception) {
        std::print(stderr, "{} {}\n", error_header(colors), exception.what());
        return EXIT_FAILURE;
    }
    catch (...) {
        std::print(stderr, "{} Caught unrecognized exception\n", error_header(colors));
        throw;
    }
}

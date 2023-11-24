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

    [[nodiscard]] auto error_stream(cppdiag::Colors const colors) -> std::ostream&
    {
        return std::cerr << colors.error.code << "Error: " << colors.normal.code;
    }

    template <void (*f)(utl::Source::Wrapper, kieli::Compile_info&)>
    auto generic_repl(cppdiag::Colors const colors)
    {
        for (;;) {
            std::string string = utl::readline(">>> ");

            if (string.empty()) {
                continue;
            }
            if (string == "q") {
                break;
            }

            utl::add_to_readline_history(string);

            auto [info, source] = kieli::test_info_and_source(std::move(string));
            try {
                f(source, info);
            }
            catch (kieli::Compilation_failure const&) {
                (void)0; // do nothing
            }
            catch (std::exception const& exception) {
                error_stream(colors) << exception.what() << "\n\n";
            }
            std::cerr << info.diagnostics.format_all(colors);
        }
    }

    constexpr auto lex_repl
        = generic_repl<[](utl::Source::Wrapper const source, kieli::Compile_info& info) {
              utl::print("Tokens: {}\n", kieli::lex(source, info));
          }>;

    constexpr auto parse_repl
        = generic_repl<[](utl::Source::Wrapper const source, kieli::Compile_info& info) {
              auto const tokens = kieli::lex(source, info);
              auto const module = kieli::parse(tokens, info);
              utl::print("{}", kieli::format_module(module, {}));
          }>;

    constexpr auto desugar_repl
        = generic_repl<[](utl::Source::Wrapper const source, kieli::Compile_info& info) {
              auto const tokens = kieli::lex(source, info);
              auto const module = kieli::desugar(kieli::parse(tokens, info), info);

              std::string output;
              for (ast::Definition const& definition : module.definitions) {
                  ast::format_to(definition, output);
              }
              utl::print("{}\n\n", output);
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
                utl::print("Total execution time: {}\n", elapsed);
            }
        } };

        if (nocolor_flag) {
            colors = cppdiag::Colors {};
        }

        if (help_flag) {
            utl::print("Valid options:\n{}", parameters.help_string());
            return EXIT_SUCCESS;
        }

        if (version_flag) {
            utl::print("kieli version 0, compiled on " __DATE__ ", " __TIME__ ".\n");
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
        std::cerr << context.format_diagnostic(diagnostic, colors);
        return EXIT_FAILURE;
    }
    catch (std::exception const& exception) {
        error_stream(colors) << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...) {
        error_stream(colors) << "Caught unrecognized exception\n";
        throw;
    }
}

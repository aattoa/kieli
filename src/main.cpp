#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libutl/readline/readline.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libformat/format.hpp>
#include <cppargs.hpp>
#include <cppdiag.hpp>

namespace {

    [[nodiscard]] auto format_command_line_error(
        cppargs::Parse_error_info const& info,
        std::string_view const           program_name,
        cppdiag::Colors const            colors) -> std::string
    {
        static constexpr auto position = [](auto const column) {
            return cppdiag::Position { .column = utl::safe_cast<std::uint32_t>(column) };
        };
        cppdiag::Message_buffer   message_buffer;
        cppdiag::Diagnostic const diagnostic {
            .text_sections { {
                .source_string  = info.command_line,
                .source_name    = "command line",
                .start_position = position(info.error_column),
                .stop_position  = position(info.error_column + info.error_width - 1),
                .note           = cppdiag::format_message(
                    message_buffer, "{}", cppargs::Parse_error_info::kind_to_string(info.kind)),
            } },
            .message   = cppdiag::format_message(message_buffer, "Command line parse failure"),
            .help_note = cppdiag::format_message(
                message_buffer,
                "To see a list of valid options, use {}{} --help{}",
                colors.hint.code,
                program_name,
                colors.normal.code),
            .severity = cppdiag::Severity::error,
        };
        return cppdiag::format_diagnostic(diagnostic, message_buffer, colors);
    }

    [[nodiscard]] auto error_header(cppdiag::Colors const& colors) -> cppdiag::Severity_header
    {
        return cppdiag::Severity_header::make(cppdiag::Severity::error, colors);
    }

    auto debug_lex(utl::Source::Wrapper const source, kieli::Compile_info& info) -> void
    {
        auto state = kieli::Lex_state::make(source, info);
        auto token = kieli::lex(state);
        while (token.type != kieli::Token::Type::end_of_input) {
            std::print("{} ", token);
            token = kieli::lex(state);
        }
        std::println("");
    }

    auto debug_parse(utl::Source::Wrapper const source, kieli::Compile_info& info) -> void
    {
        auto const module = kieli::parse(source, info);
        std::print("{}", kieli::format_module(module, kieli::Format_configuration {}));
    }

    auto debug_desugar(utl::Source::Wrapper const source, kieli::Compile_info& info) -> void
    {
        auto const  module = kieli::desugar(kieli::parse(source, info), info);
        std::string output;
        for (ast::Definition const& definition : module.definitions) {
            ast::format_to(definition, output);
        }
        std::print("{}\n\n", output);
    }

    auto debug_resolve(utl::Source::Wrapper const source, kieli::Compile_info& info) -> void
    {
        auto arenas    = libresolve::Arenas::defaults();
        auto constants = libresolve::Constants::make_with(arenas);

        kieli::Project_configuration configuration;

        libresolve::Context context {
            .arenas                = std::move(arenas),
            .constants             = std::move(constants),
            .project_configuration = configuration,
            .compile_info          = info,
        };

        auto const environment = libresolve::make_environment(context, source);
        libresolve::resolve_definitions_in_order(context, environment);
        libresolve::resolve_function_bodies(context, environment);
    }

    auto choose_debug_repl_callback(std::string_view const name)
        -> void (*)(utl::Source::Wrapper, kieli::Compile_info&)
    {
        // clang-format off
        if (name == "lex") return debug_lex;
        if (name == "par") return debug_parse;
        if (name == "des") return debug_desugar;
        if (name == "res") return debug_resolve;
        return nullptr;
        // clang-format on
    }

    auto run_debug_repl(
        void (&callback)(utl::Source::Wrapper, kieli::Compile_info&),
        cppdiag::Colors const colors) -> void
    {
        while (auto input = utl::readline(">>> ")) {
            if (input == "q") {
                return;
            }
            if (input.value().find_first_not_of(' ') == std::string::npos) {
                continue;
            }

            utl::add_to_readline_history(input.value());

            auto [info, source] = kieli::test_info_and_source(std::move(input.value()));
            try {
                callback(source, info);
            }
            catch (kieli::Compilation_failure const&) {
                (void)0; // Do nothing, diagnostics are printed below
            }
            catch (std::exception const& exception) {
                std::print(stderr, "{}{}\n\n", error_header(colors), exception.what());
            }

            std::print(stderr, "{}", info.diagnostics.format_all(colors));
        }
    }

    auto choose_and_run_debug_repl(std::string_view const name, cppdiag::Colors const colors) -> int
    {
        if (auto* const callback = choose_debug_repl_callback(name)) {
            run_debug_repl(*callback, colors);
            return EXIT_SUCCESS;
        }
        std::println(stderr, "{}Unrecognized REPL name: '{}'", error_header(colors), name);
        return EXIT_FAILURE;
    }

} // namespace

auto main(int const argc, char const* const* const argv) -> int
{
    cppargs::Parameters parameters;

    auto const help_flag    = parameters.add('h', "help", "Show this help text");
    auto const version_flag = parameters.add('v', "version", "Show Kieli version");
    auto const nocolor_flag = parameters.add("nocolor", "Disable colored output");
    auto const repl_option  = parameters.add<std::string_view>("repl", "Run the given REPL");

    cppdiag::Colors colors = cppdiag::Colors::defaults();

    try {
        cppargs::parse(argc, argv, parameters);

        if (nocolor_flag) {
            colors = cppdiag::Colors::none();
        }
        if (version_flag) {
            std::println("Kieli version 0");
        }
        if (help_flag) {
            std::print("Valid options:\n{}", parameters.help_string());
        }
        if (repl_option) {
            return choose_and_run_debug_repl(repl_option.value(), colors);
        }

        return EXIT_SUCCESS;
    }
    catch (cppargs::Exception const& exception) {
        auto const name = *argv ? *argv : "kieli";
        std::println(stderr, "{}", format_command_line_error(exception.info(), name, colors));
        return EXIT_FAILURE;
    }
    catch (std::exception const& exception) {
        std::println(stderr, "{}{}", error_header(colors), exception.what());
        return EXIT_FAILURE;
    }
    catch (...) {
        std::println(stderr, "{}Caught unrecognized exception", error_header(colors));
        throw;
    }
}

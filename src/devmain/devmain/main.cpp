#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libformat/format.hpp>
#include <cpputil/input.hpp>
#include <cppargs.hpp>
#include <devmain/history.hpp>

namespace {

    [[nodiscard]] auto format_command_line_error(
        cppargs::Parse_error_info const& info,
        std::string_view const           program_name,
        cppdiag::Colors const            colors) -> std::string
    {
        static constexpr auto position = [](auto const column) {
            return cppdiag::Position { .column = utl::safe_cast<std::uint32_t>(column) - 1 };
        };
        cppdiag::Diagnostic const diagnostic {
            .text_sections = utl::to_vector({
                cppdiag::Text_section {
                    .source_string  = info.command_line,
                    .source_name    = "command line",
                    .start_position = position(info.error_column),
                    .stop_position  = position(info.error_column + info.error_width),
                    .note = std::string(cppargs::Parse_error_info::kind_to_string(info.kind)),
                },
            }),
            .message       = "Command line parse failure",
            .help_note     = std::format(
                "To see a list of valid options, use {}{} --help{}",
                colors.hint.code,
                program_name,
                colors.normal.code),
            .severity = cppdiag::Severity::error,
        };
        return cppdiag::format_diagnostic(diagnostic, colors);
    }

    [[nodiscard]] auto error_header(cppdiag::Colors const& colors) -> cppdiag::Severity_header
    {
        return cppdiag::Severity_header::make(cppdiag::Severity::error, colors);
    }

    auto debug_lex(kieli::Database& db, kieli::Document_id const document_id) -> void
    {
        auto state = kieli::lex_state(db, document_id);
        auto token = kieli::lex(state);
        while (token.type != kieli::Token_type::end_of_input) {
            std::print("{} ", token);
            token = kieli::lex(state);
        }
        std::println("");
    }

    auto debug_parse(kieli::Database& db, kieli::Document_id const document_id) -> void
    {
        auto const cst = kieli::parse(db, document_id);
        std::print("{}", kieli::format_module(cst.get(), kieli::Format_configuration {}));
    }

    auto debug_desugar(kieli::Database& db, kieli::Document_id const document_id) -> void
    {
        auto const cst = kieli::parse(db, document_id);
        for (kieli::cst::Definition const& definition : cst.get().definitions) {
            kieli::ast::Arena ast;
            (void)kieli::desugar(db, document_id, ast, cst.get().arena, definition); // todo
        }
    }

    auto debug_resolve(kieli::Database& db, kieli::Document_id const document_id) -> void
    {
        auto hir       = kieli::hir::Arena {};
        auto constants = libresolve::Constants::make_with(hir);

        libresolve::Context context {
            .db        = db,
            .hir       = std::move(hir),
            .constants = std::move(constants),
        };

        auto const environment = libresolve::make_environment(context, document_id);
        libresolve::resolve_definitions_in_order(context, environment);
        libresolve::resolve_function_bodies(context, environment);
    }

    auto choose_debug_repl_callback(std::string_view const name)
        -> void (*)(kieli::Database&, kieli::Document_id)
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
        void (&callback)(kieli::Database&, kieli::Document_id),
        cppdiag::Colors const colors) -> void
    {
        kieli::read_history_file_to_active_history();

        while (auto input = cpputil::input::read_line(">>> ")) {
            if (input == "q") {
                return;
            }
            if (input.value().find_first_not_of(' ') == std::string::npos) {
                continue;
            }

            kieli::add_to_history(input.value().c_str());

            auto       db          = kieli::Database {};
            auto const path        = std::filesystem::path("[repl]");
            auto const document_id = kieli::add_document(db, path, std::move(input).value());

            try {
                callback(db, document_id);
            }
            catch (kieli::Compilation_failure const&) {
                (void)0; // Do nothing, diagnostics are printed below
            }
            catch (std::exception const& exception) {
                std::print(stderr, "{}{}\n\n", error_header(colors), exception.what());
            }

            auto const& document = kieli::document(db, document_id);
            std::print(stderr, "{}", kieli::format_diagnostics(path, document, colors));
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

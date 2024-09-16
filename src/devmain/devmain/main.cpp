#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libformat/format.hpp>
#include <libcompiler/ast/display.hpp>
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
        std::print("{}", kieli::format_module(cst.get(), kieli::Format_options {}));
    }

    auto debug_desugar(kieli::Database& db, kieli::Document_id const document_id) -> void
    {
        auto const cst_module = kieli::parse(db, document_id);
        for (kieli::cst::Definition const& cst_definition : cst_module.get().definitions) {
            kieli::ast::Arena            ast_arena;
            kieli::ast::Definition const ast_definition = kieli::desugar(
                db, document_id, ast_arena, cst_module.get().arena, cst_definition);
            std::println("{}", kieli::ast::display(ast_arena, ast_definition));
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

        (void)context;
        (void)document_id;
        cpputil::todo();
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

    auto wrap_exceptions(std::invocable auto const& callback, cppdiag::Colors const colors) -> void
    {
        try {
            std::invoke(callback);
        }
        catch (std::exception const& exception) {
            std::print(stderr, "{}{}\n\n", error_header(colors), exception.what());
        }
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
            kieli::Database db { .manifest { .root_path = std::filesystem::current_path() } };
            auto const      document_id = kieli::test_document(db, std::move(input).value());
            wrap_exceptions([&] { callback(db, document_id); }, colors);
            std::print("{}", kieli::format_document_diagnostics(db, document_id, colors));
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

    auto dump(
        std::string_view const                                           filename,
        cppdiag::Colors const                                            colors,
        std::invocable<kieli::Database&, kieli::Document_id> auto const& callback) -> int
    {
        kieli::Database db;
        if (auto document_id = kieli::read_document(db, filename)) {
            wrap_exceptions([&] { callback(db, document_id.value()); }, colors);
            std::print("{}", kieli::format_document_diagnostics(db, document_id.value(), colors));
            return EXIT_SUCCESS;
        }
        else {
            std::println(
                stderr,
                "{}Failed to read '{}': {}",
                error_header(colors),
                filename,
                kieli::describe_read_failure(document_id.error()));
            return EXIT_FAILURE;
        }
    }

} // namespace

auto main(int const argc, char const* const* const argv) -> int
{
    cppargs::Parameters parameters;

    auto const help_flag    = parameters.add('h', "help", "Show this help text");
    auto const version_flag = parameters.add('v', "version", "Show Kieli version");
    auto const nocolor_flag = parameters.add("nocolor", "Disable colored output");
    auto const cst_flag     = parameters.add<std::string_view>("cst", "Dump the CST of a file");
    auto const ast_flag     = parameters.add<std::string_view>("ast", "Dump the AST of a file");
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
        if (cst_flag) {
            return dump(cst_flag.value(), colors, debug_parse);
        }
        if (ast_flag) {
            return dump(ast_flag.value(), colors, debug_desugar);
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

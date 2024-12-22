#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>
#include <libformat/format.hpp>
#include <libcompiler/ast/display.hpp>
#include <devmain/history.hpp>
#include <cpputil/input.hpp>

namespace {
    [[nodiscard]] auto error_header(cppdiag::Colors colors) -> cppdiag::Severity_header
    {
        return cppdiag::Severity_header::make(cppdiag::Severity::error, colors);
    }

    void debug_lex(kieli::Database& db, kieli::Document_id id)
    {
        auto state = kieli::lex_state(db, id);
        auto token = kieli::lex(state);
        while (token.type != kieli::Token_type::end_of_input) {
            std::print("{} ", token);
            token = kieli::lex(state);
        }
        std::println("");
    }

    void debug_parse(kieli::Database& db, kieli::Document_id id)
    {
        auto const cst = kieli::parse(db, id);
        std::print("{}", kieli::format_module(cst.get(), kieli::Format_options {}));
    }

    void debug_desugar(kieli::Database& db, kieli::Document_id id)
    {
        auto const             cst = kieli::parse(db, id);
        kieli::ast::Arena      arena;
        kieli::Desugar_context context { db, cst.get().arena, arena, id };
        for (kieli::cst::Definition const& definition : cst.get().definitions) {
            std::println("{}", display(arena, kieli::desugar_definition(context, definition)));
        }
    }

    void debug_resolve(kieli::Database& db, kieli::Document_id id)
    {
        auto hir       = kieli::hir::Arena {};
        auto constants = libresolve::make_constants(hir);

        auto context = libresolve::Context {
            .db        = db,
            .hir       = std::move(hir),
            .constants = std::move(constants),
        };
        auto env_id = libresolve::collect_document(context, id);
        libresolve::resolve_environment(context, env_id);
        libresolve::debug_display_environment(context, env_id);
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

    void wrap_exceptions(std::invocable auto const& callback, cppdiag::Colors colors)
    {
        try {
            std::invoke(callback);
        }
        catch (std::exception const& exception) {
            std::print(stderr, "{}{}\n\n", error_header(colors), exception.what());
        }
    }

    void run_debug_repl(
        void (&callback)(kieli::Database&, kieli::Document_id), cppdiag::Colors colors)
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
            auto const      id = kieli::test_document(db, std::move(input).value());
            wrap_exceptions([&] { callback(db, id); }, colors);
            std::print("{}", kieli::format_document_diagnostics(db, id, colors));
        }
    }

    auto choose_and_run_repl(std::string_view name, cppdiag::Colors colors) -> int
    {
        if (auto* const callback = choose_debug_repl_callback(name)) {
            run_debug_repl(*callback, colors);
            return EXIT_SUCCESS;
        }
        std::println(stderr, "{}Unrecognized REPL name: '{}'", error_header(colors), name);
        return EXIT_FAILURE;
    }

    auto dump(std::string_view filename, cppdiag::Colors colors, auto const& callback) -> int
    {
        kieli::Database db;
        if (auto id = kieli::read_document(db, filename)) {
            wrap_exceptions([&] { callback(db, id.value()); }, colors);
            std::print("{}", kieli::format_document_diagnostics(db, id.value(), colors));
            return EXIT_SUCCESS;
        }
        else {
            std::println(
                stderr,
                "{}Failed to read '{}': {}",
                error_header(colors),
                filename,
                kieli::describe_read_failure(id.error()));
            return EXIT_FAILURE;
        }
    }

    template <class... Args>
    auto error(std::format_string<Args...> fmt, Args&&... args) -> int
    {
        std::println(stderr, fmt, std::forward<Args>(args)...);
        return EXIT_FAILURE;
    }

    auto bad_argument(char const* argument, char const* program, cppdiag::Colors colors) -> int
    {
        return error(
            "Unrecognized argument: '{}'\n\nFor help, use {}{} --help{}",
            argument,
            colors.hint.code,
            program,
            colors.normal.code);
    }

    auto const help_string = R"(Valid options:
    --help, -h       Show this help text
    --version, -v    Show version information
    --nocolor        Disable colored output
    cst [file]       Dump the CST for the given file
    ast [file]       Dump the AST for the given file
    repl [name]      Run the given REPL)";
} // namespace

auto main(int const argc, char const* const* const argv) -> int
{
    cppdiag::Colors colors  = cppdiag::Colors::defaults();
    const char*     program = *argv ? *argv : "kieli";

    try {
        for (auto arg = argv + 1; arg != argv + argc; ++arg) {
            auto const next = [&] { return ++arg != argv + argc; };

            if (*arg == "--nocolor"sv) {
                colors = cppdiag::Colors::none();
            }
            else if (*arg == "-v"sv || *arg == "--version"sv) {
                std::println("Kieli version 0");
            }
            else if (*arg == "-h"sv || *arg == "--help"sv) {
                std::println("Usage: {} [options]\n{}", program, help_string);
            }
            else if (*arg == "cst"sv) {
                return next() ? dump(*arg, colors, debug_parse) : error("Missing file path");
            }
            else if (*arg == "ast"sv) {
                return next() ? dump(*arg, colors, debug_desugar) : error("Missing file path");
            }
            else if (*arg == "repl"sv) {
                return next() ? choose_and_run_repl(*arg, colors) : error("Missing REPL name");
            }
            else {
                return bad_argument(*arg, program, colors);
            }
        }
        return EXIT_SUCCESS;
    }
    catch (std::exception const& exception) {
        std::println(stderr, "{}Unhandled exception: {}", error_header(colors), exception.what());
        return EXIT_FAILURE;
    }
    catch (...) {
        std::println(stderr, "{}Caught unrecognized exception", error_header(colors));
        throw;
    }
}

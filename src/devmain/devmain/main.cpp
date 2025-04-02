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
    void debug_lex(ki::Database& db, ki::Document_id id)
    {
        auto state = ki::lex::state(db.documents[id].text);
        auto token = ki::lex::next(state);
        while (token.type != ki::Token_type::End_of_input) {
            std::print("{} ", ki::token_type_string(token.type));
            token = ki::lex::next(state);
        }
        std::println("");
    }

    void debug_parse(ki::Database& db, ki::Document_id id)
    {
        auto cst = ki::parse::parse(db, id);
        auto mod = ki::format::format_module(cst, db.string_pool, ki::format::Options {});
        std::print("{}", mod);
    }

    void debug_desugar(ki::Database& db, ki::Document_id id)
    {
        auto ast = ki::ast::Arena {};
        auto cst = ki::parse::parse(db, id);

        ki::desugar::Context ctx { .db = db, .cst = cst.arena, .ast = ast, .doc_id = id };
        for (ki::cst::Definition const& definition : cst.definitions) {
            auto def = ki::desugar::desugar_definition(ctx, definition);
            std::println("{}", ki::ast::display(ast, db.string_pool, def));
        }
    }

    void debug_resolve(ki::Database& db, ki::Document_id id)
    {
        auto ctx = ki::resolve::context(db);
        auto env = ki::resolve::collect_document(ctx, id);
        ki::resolve::resolve_environment(ctx, env);
        ki::resolve::debug_display_environment(ctx, env);
    }

    auto choose_debug_repl_callback(std::string_view const name)
        -> void (*)(ki::Database&, ki::Document_id)
    {
        // clang-format off
        if (name == "lex") return debug_lex;
        if (name == "par") return debug_parse;
        if (name == "des") return debug_desugar;
        if (name == "res") return debug_resolve;
        return nullptr;
        // clang-format on
    }

    void wrap_exceptions(std::invocable auto const& callback)
    {
        try {
            std::invoke(callback);
        }
        catch (std::exception const& exception) {
            std::print(stderr, "Error: {}\n\n", exception.what());
        }
    }

    void run_debug_repl(void (&callback)(ki::Database&, ki::Document_id))
    {
        ki::read_history_file_to_active_history();

        while (auto input = cpputil::input::read_line(">>> ")) {
            if (input == "q") {
                return;
            }
            if (input.value().find_first_not_of(' ') == std::string::npos) {
                continue;
            }
            ki::add_to_history(input.value().c_str());

            auto db = ki::database({ .root_path = std::filesystem::current_path() });
            auto id = ki::test_document(db, std::move(input).value());
            wrap_exceptions([&] { callback(db, id); });
            ki::print_diagnostics(stderr, db, id);
        }
    }

    auto choose_and_run_repl(std::string_view name) -> int
    {
        if (auto* const callback = choose_debug_repl_callback(name)) {
            run_debug_repl(*callback);
            return EXIT_SUCCESS;
        }
        std::println(stderr, "Error: Unrecognized REPL name: '{}'", name);
        return EXIT_FAILURE;
    }

    auto dump(std::string_view filename, auto const& callback) -> int
    {
        ki::Database db;
        if (auto id = ki::read_document(db, filename)) {
            wrap_exceptions([&] { callback(db, id.value()); });
            ki::print_diagnostics(stderr, db, id.value());
            return EXIT_SUCCESS;
        }
        else {
            std::println(
                stderr,
                "Error: Failed to read '{}': {}",
                filename,
                ki::describe_read_failure(id.error()));
            return EXIT_FAILURE;
        }
    }

    template <typename... Args>
    auto error(std::format_string<Args...> fmt, Args&&... args) -> int
    {
        std::println(stderr, fmt, std::forward<Args>(args)...);
        return EXIT_FAILURE;
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
    const char* program = *argv ? *argv : "kieli";

    try {
        for (char const* const* arg = argv + 1; arg != argv + argc; ++arg) {
            auto const next = [&] { return ++arg != argv + argc; };

            if (*arg == "-v"sv or *arg == "--version"sv) {
                std::println("Kieli version 0");
            }
            else if (*arg == "-h"sv or *arg == "--help"sv) {
                std::println("Usage: {} [options]\n{}", program, help_string);
            }
            else if (*arg == "cst"sv) {
                return next() ? dump(*arg, debug_parse) : error("Missing file path");
            }
            else if (*arg == "ast"sv) {
                return next() ? dump(*arg, debug_desugar) : error("Missing file path");
            }
            else if (*arg == "repl"sv) {
                return next() ? choose_and_run_repl(*arg) : error("Missing REPL name");
            }
            else {
                return error("Unrecognized option: '{}'\n\nFor help, use {} --help", *arg, program);
            }
        }
        return EXIT_SUCCESS;
    }
    catch (std::exception const& exception) {
        std::println(stderr, "Error: Unhandled exception: {}", exception.what());
        return EXIT_FAILURE;
    }
    catch (...) {
        std::println(stderr, "Error: Caught unrecognized exception");
        throw;
    }
}

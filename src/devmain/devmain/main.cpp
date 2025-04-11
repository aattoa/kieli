#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>
#include <libformat/format.hpp>
#include <libcompiler/ast/display.hpp>
#include <devmain/repl.hpp>
#include <cpputil/input.hpp>

using namespace ki;

namespace {
    void debug_lex(db::Database& db, db::Document_id id)
    {
        auto state = lex::state(db.documents[id].text);
        auto token = lex::next(state);
        while (token.type != lex::Type::End_of_input) {
            std::print("{} ", lex::token_type_string(token.type));
            token = lex::next(state);
        }
        std::println("");
    }

    void debug_parse(db::Database& db, db::Document_id id)
    {
        auto text = fmt::format_module(
            db.string_pool, db.documents[id].cst, fmt::Options {}, par::parse(db, id));
        std::print("{}", text);
    }

    void debug_desugar(db::Database& db, db::Document_id id)
    {
        auto mod = par::parse(db, id);
        auto ctx = des::context(db, id);
        for (auto const& cst : mod.definitions) {
            auto ast = des::desugar_definition(ctx, cst);
            std::println("{}", ast::display(ctx.ast, db.string_pool, ast));
        }
    }

    void debug_resolve(db::Database& db, db::Document_id id)
    {
        auto ctx = res::context(db);
        auto env = res::collect_document(ctx, id);
        res::resolve_environment(ctx, env);
        res::debug_display_environment(ctx, env);
    }

    auto choose_debug_repl_callback(std::string_view const name)
        -> void (*)(db::Database&, db::Document_id)
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

    void run_debug_repl(void (&callback)(db::Database&, db::Document_id))
    {
        repl::read_history_file();

        while (auto input = cpputil::input::read_line(">>> ")) {
            if (input == "q") {
                return;
            }
            if (input.value().find_first_not_of(' ') == std::string::npos) {
                continue;
            }
            repl::add_history_line(input.value().c_str());

            auto db = db::database({ .root_path = std::filesystem::current_path() });
            auto id = db::test_document(db, std::move(input).value());
            wrap_exceptions([&] { callback(db, id); });
            db::print_diagnostics(stderr, db, id);
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
        db::Database db;
        if (auto id = db::read_document(db, filename)) {
            wrap_exceptions([&] { callback(db, id.value()); });
            db::print_diagnostics(stderr, db, id.value());
            return EXIT_SUCCESS;
        }
        else {
            std::println(
                stderr,
                "Error: Failed to read '{}': {}",
                filename,
                db::describe_read_failure(id.error()));
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

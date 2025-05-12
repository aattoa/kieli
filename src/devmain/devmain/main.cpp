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
    void debug_lex(db::Database& db, db::Document_id doc_id)
    {
        auto state = lex::state(db.documents[doc_id].text);
        auto token = lex::next(state);
        while (token.type != lex::Type::End_of_input) {
            std::print("{} ", lex::token_type_string(token.type));
            token = lex::next(state);
        }
        std::println("");
    }

    void debug_parse(db::Database& db, db::Document_id doc_id)
    {
        auto text = fmt::format_module(
            db.string_pool,
            db.documents[doc_id].arena.cst,
            fmt::Options {},
            par::parse(db, doc_id));
        std::print("{}", text);
    }

    void debug_desugar(db::Database& db, db::Document_id doc_id)
    {
        auto mod = par::parse(db, doc_id);
        auto ctx = des::context(db, doc_id);
        for (auto const& cst : mod.definitions) {
            auto ast = des::desugar_definition(ctx, cst);
            std::println("{}", ast::display(ctx.ast, db.string_pool, ast));
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

            auto db     = db::database({ .root_path = std::filesystem::current_path() });
            auto doc_id = db::test_document(db, std::move(input).value());

            try {
                callback(db, doc_id);
            }
            catch (std::exception const& exception) {
                std::print(std::cerr, "Error: {}\n\n", exception.what());
            }

            db::print_diagnostics(std::cerr, db, doc_id);
        }
    }

    auto choose_and_run_repl(std::string_view name) -> int
    {
        if (name == "lex") {
            run_debug_repl(debug_lex);
            return EXIT_SUCCESS;
        }
        if (name == "par") {
            run_debug_repl(debug_parse);
            return EXIT_SUCCESS;
        }
        if (name == "des") {
            run_debug_repl(debug_desugar);
            return EXIT_SUCCESS;
        }
        std::println(std::cerr, "Error: Unrecognized REPL name: '{}'", name);
        return EXIT_FAILURE;
    }

    auto dump(std::string_view filename, auto const& callback) -> int
    {
        db::Database db;
        if (auto doc_id = db::read_document(db, filename)) {
            try {
                callback(db, doc_id.value());
            }
            catch (std::exception const& exception) {
                std::print(std::cerr, "Error: {}\n\n", exception.what());
            }
            db::print_diagnostics(std::cerr, db, doc_id.value());
            return EXIT_SUCCESS;
        }
        else {
            std::println(
                std::cerr,
                "Error: Failed to read '{}': {}",
                filename,
                db::describe_read_failure(doc_id.error()));
            return EXIT_FAILURE;
        }
    }

    template <typename... Args>
    auto error(std::format_string<Args...> fmt, Args&&... args) -> int
    {
        std::println(std::cerr, fmt, std::forward<Args>(args)...);
        return EXIT_FAILURE;
    }

    auto const help_string = R"(Valid options:
    --help, -h       Show this help text
    --version, -v    Show version information
    cst [file]       Dump the CST for the given file
    ast [file]       Dump the AST for the given file
    repl [name]      Run the given REPL)";
} // namespace

auto main(int argc, char const* const* argv) -> int
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
        std::println(std::cerr, "Error: Unhandled exception: {}", exception.what());
        return EXIT_FAILURE;
    }
    catch (...) {
        std::println(std::cerr, "Error: Caught unrecognized exception");
        throw;
    }
}

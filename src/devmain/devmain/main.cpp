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
    void analyze_document(db::Database& db, db::Document_id doc_id)
    {
        auto ctx = res::context(doc_id);

        db.documents[doc_id].info = {
            .diagnostics     = {},
            .semantic_tokens = {},
            .inlay_hints     = {},
            .references      = {},
            .actions         = {},
            .root_env_id     = ctx.root_env_id,
            .signature_info  = std::nullopt,
            .completion_info = std::nullopt,
        };

        auto symbol_ids = res::collect_document(db, ctx);

        for (db::Symbol_id symbol_id : symbol_ids) {
            res::resolve_symbol(db, ctx, symbol_id);
        }
        for (db::Symbol_id symbol_id : symbol_ids) {
            res::warn_if_unused(db, ctx, symbol_id);
        }

        db.documents[doc_id].arena = std::move(ctx.arena);
    }

    auto check(std::string_view filename) -> int
    {
        auto db = db::database({});
        if (auto doc_id = db::read_document(db, filename)) {
            analyze_document(db, doc_id.value());
            db::print_diagnostics(std::cout, db, doc_id.value());
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
            else if (*arg == "check"sv) {
                return next() ? check(*arg) : error("Missing file path");
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

#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>
#include <libformat/format.hpp>
#include <libdisplay/display.hpp>

using namespace ki;

namespace {
    template <typename... Args>
    [[noreturn]] void die(std::format_string<Args...> fmt, Args&&... args)
    {
        std::println(std::cerr, fmt, std::forward<Args>(args)...);
        std::quick_exit(EXIT_FAILURE);
    }

    auto read_document(db::Database& db, std::string_view path) -> db::Document_id
    {
        if (auto doc_id = db::read_document(db, path)) {
            return doc_id.value();
        }
        else {
            die("Error: {}: '{}'", db::describe_read_failure(doc_id.error()), path);
        }
    }

    void check(std::string_view path)
    {
        auto db     = db::database({});
        auto doc_id = read_document(db, path);
        auto sink   = db::Diagnostic_stream_sink(db, std::cout);
        auto ctx    = res::context(doc_id, sink);

        db.documents[doc_id].info.root_env_id = ctx.root_env_id;

        auto symbol_ids = res::collect_document(db, ctx);

        for (db::Symbol_id symbol_id : symbol_ids) {
            res::resolve_symbol(db, ctx, symbol_id);
        }
        for (db::Symbol_id symbol_id : symbol_ids) {
            res::warn_if_unused(db, ctx, symbol_id);
        }
    }

    void parse(std::string_view path)
    {
        auto db     = db::database({});
        auto doc_id = read_document(db, path);
        auto sink   = db::Diagnostic_stream_sink(db, std::cout);
        auto ctx    = par::context(db, doc_id, sink);
        par::parse(ctx, [](auto const&) {});
    }

    void format(std::string_view path)
    {
        auto db     = db::database({});
        auto doc_id = read_document(db, path);
        auto sink   = db::Diagnostic_stream_sink(db, std::cerr);
        fmt::format_document(std::cout, db, doc_id, sink, fmt::Options {});
    }

    void dump_ast(std::string_view path)
    {
        auto db     = db::database({});
        auto doc_id = read_document(db, path);
        auto sink   = db::Diagnostic_stream_sink(db, std::cerr);
        dis::display_document(std::cout, db, doc_id, sink);
    }

    auto const help_text = R"(Usage: kieli [OPTIONS] [COMMAND]

Options:
    -v, --version   Show version information
    -h, --help      Show this help text

Commands:
    check [PATH]    Analyze the given document and print diagnostics
    parse [PATH]    Just parse the given document and print diagnostics
    fmt [PATH]      Format the given document to standard output
    ast [PATH]      Parse and desugar the given document and display its AST)";
} // namespace

auto main(int argc, char const* const* argv) -> int
{
    auto next = [=, arg = argv + 1](std::string_view desc) mutable {
        if (arg != argv + argc) {
            return std::string_view(*arg++);
        }
        die("Missing required argument {}", desc);
    };

    try {
        if (argc == 1) {
            std::println("{}", help_text);
            return EXIT_SUCCESS;
        }

        auto arg = next("[ARG]");

        if (arg == "-v" or arg == "--version") {
            std::println("Kieli 0.1.0");
        }
        else if (arg == "-h" or arg == "--help") {
            std::println("{}", help_text);
        }
        else if (arg == "check") {
            check(next("[PATH]"));
        }
        else if (arg == "parse") {
            parse(next("[PATH]"));
        }
        else if (arg == "fmt" or arg == "format") {
            format(next("[PATH]"));
        }
        else if (arg == "ast") {
            dump_ast(next("[PATH]"));
        }
        else {
            char const* desc = arg.starts_with('-') ? "option" : "command";
            die("Unrecognized {}: '{}'\n\nFor help, try 'kieli --help'", desc, arg);
        }

        return EXIT_SUCCESS;
    }
    catch (db::Max_errors_reached const& error) {
        std::println(std::cerr, "{} errors occurred, stopping compilation", error.count);
    }
    catch (std::exception const& exception) {
        std::println(std::cerr, "Error: Unhandled exception: {}", exception.what());
    }
    catch (...) {
        std::println(std::cerr, "Error: Caught unrecognized exception");
    }

    return EXIT_FAILURE;
}

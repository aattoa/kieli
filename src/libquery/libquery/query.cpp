#include <libutl/utilities.hpp>
#include <libquery/query.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>

auto kieli::query::source(Database& db, std::filesystem::path path)
    -> std::expected<Source_id, Read_failure>
{
    auto const source = find_source(path, db.sources);
    return source ? source.value() : read_source(std::move(path), db.sources);
}

auto kieli::query::cst(Database& db, Source_id const source) -> std::optional<CST>
{
    // TODO: get rid of Compilation_failure
    try {
        return parse(source, db);
    }
    catch (Compilation_failure const&) {
        return std::nullopt;
    }
}

auto kieli::query::ast(Database& db, CST const& cst) -> std::optional<AST>
{
    // TODO: get rid of Compilation_failure
    try {
        return desugar(cst, db);
    }
    catch (Compilation_failure const&) {
        return std::nullopt;
    }
}

auto kieli::query::hover(Database& db, Location const location) -> std::optional<std::string>
{
    (void)db;
    (void)location;
    return "hello, world!";
}

auto kieli::query::definition(Database& db, Location location) -> std::optional<Location>
{
    (void)db;
    ++location.range.start.column;
    ++location.range.stop.column;
    return location;
}

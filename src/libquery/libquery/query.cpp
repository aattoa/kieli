#include <libutl/utilities.hpp>
#include <libquery/query.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>

auto kieli::query::source(Database& db, std::filesystem::path const& path) -> Result<Source_id>
{
    if (auto const source = find_source(path, db.sources)) {
        return source.value();
    }
    return read_source(path, db.sources).transform_error([&](Read_failure const failure) {
        return std::format("{}: '{}'", describe_read_failure(failure), path.c_str());
    });
}

auto kieli::query::cst(Database& db, Source_id const source) -> Result<CST>
{
    // TODO: get rid of Compilation_failure
    try {
        return parse(source, db);
    }
    catch (Compilation_failure const&) {
        return std::unexpected("cst query failed"s);
    }
}

auto kieli::query::ast(Database& db, CST const& cst) -> Result<AST>
{
    // TODO: get rid of Compilation_failure
    try {
        return desugar(cst, db);
    }
    catch (Compilation_failure const&) {
        return std::unexpected("ast query failed"s);
    }
}

auto kieli::query::hover(Database& db, Location const location)
    -> Result<std::optional<std::string>>
{
    return std::format("hello, world!\n\nfile: `{}`", db.sources[location.source].path.c_str());
}

auto kieli::query::definition(Database& db, Location location) -> Result<Location>
{
    (void)db;
    ++location.range.start.column;
    ++location.range.stop.column;
    return location;
}

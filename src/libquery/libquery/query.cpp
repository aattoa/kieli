#include <libutl/utilities.hpp>
#include <libquery/query.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>

auto kieli::query::document_id(Database& db, std::filesystem::path const& path)
    -> Result<Document_id>
{
    if (auto const document_id = find_document(db, path)) {
        return document_id.value();
    }
    return read_document(db, path).transform_error([&](Read_failure const failure) {
        return std::format("{}: '{}'", describe_read_failure(failure), path.c_str());
    });
}

auto kieli::query::cst(Database& db, Document_id const source) -> Result<CST>
{
    // TODO: get rid of Compilation_failure
    try {
        return parse(db, source);
    }
    catch (Compilation_failure const&) {
        return std::unexpected("cst query failed"s);
    }
}

auto kieli::query::ast(Database& db, CST const& cst) -> Result<AST>
{
    // TODO: get rid of Compilation_failure
    try {
        return desugar(db, cst);
    }
    catch (Compilation_failure const&) {
        return std::unexpected("ast query failed"s);
    }
}

auto kieli::query::hover(Database& db, Location const location)
    -> Result<std::optional<std::string>>
{
    return std::format("hello, world!\n\nfile: `{}`", db.paths[location.document_id].c_str());
}

auto kieli::query::definition(Database& db, Location location) -> Result<Location>
{
    (void)db;
    ++location.range.start.column;
    ++location.range.stop.column;
    return location;
}

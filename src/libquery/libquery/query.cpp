#include <libutl/utilities.hpp>
#include <libquery/query.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>

auto kieli::query::document_id(Database& db, std::filesystem::path const& path)
    -> Result<Document_id>
{
    if (auto const document_id = find_existing_document_id(db, path)) {
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

auto kieli::query::hover(Database& db, Character_location const location)
    -> Result<std::optional<std::string>>
{
    return std::format(
        "Hello, world!\n\nFile: `{}`\nPosition: {}",
        db.paths[location.document_id].c_str(),
        location.position);
}

auto kieli::query::definition(Database& /*db*/, Character_location location) -> Result<Location>
{
    ++location.position.column;
    return Location {
        .document_id = location.document_id,
        .range       = Range::for_position(location.position),
    };
}

#include <libutl/utilities.hpp>
#include <language-server/rpc.hpp>

void ki::lsp::rpc::write_message(std::ostream& out, std::string_view const message)
{
    std::format_to(
        std::ostreambuf_iterator(out), "Content-Length: {}\r\n\r\n{}", message.size(), message);
}

auto ki::lsp::rpc::read_message(std::istream& in) -> std::expected<std::string, std::string_view>
{
    if (not in.ignore("Content-Length: "sv.size())) {
        return std::unexpected("Missing Content-Length header"sv);
    }

    std::streamsize content_length {};
    if (in >> content_length; not in) {
        return std::unexpected("Missing content length"sv);
    }

    if (not in.ignore("\r\n\r\n"sv.size())) {
        return std::unexpected("Missing content separator"sv);
    }

    std::string content;
    content.resize(content_length);

    if (not in.read(&content.front(), content_length)) {
        return std::unexpected("Premature end of input"sv);
    }

    return content;
}

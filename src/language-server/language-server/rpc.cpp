#include <libutl/utilities.hpp>
#include <language-server/rpc.hpp>

auto kieli::lsp::rpc_write_message(std::ostream& out, std::string_view const message) -> void
{
    std::format_to(
        std::ostreambuf_iterator(out), "Content-Length: {}\r\n\r\n{}", message.size(), message);
}

auto kieli::lsp::rpc_read_message(std::istream& in) -> std::expected<std::string, Read_failure>
{
    if (!in.ignore("Content-Length: "sv.size())) {
        return std::unexpected(Read_failure::no_header);
    }

    std::streamsize content_length {};
    if (in >> content_length; !in) {
        return std::unexpected(Read_failure::no_length);
    }

    if (!in.ignore("\r\n\r\n"sv.size())) {
        return std::unexpected(Read_failure::no_separator);
    }

    std::string content;
    content.resize(content_length);

    if (!in.read(&content.front(), content_length)) {
        return std::unexpected(Read_failure::no_content);
    }

    return content;
}

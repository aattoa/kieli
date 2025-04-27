#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include <language-server/rpc.hpp>
#include <sstream>

using namespace ki;

UNITTEST("RPC communication")
{
    std::stringstream stream;

    lsp::rpc::write_message(stream, "hello");
    lsp::rpc::write_message(stream, "world!");

    REQUIRE_EQUAL(stream.view(), "Content-Length: 5\r\n\r\nhelloContent-Length: 6\r\n\r\nworld!");

    REQUIRE_EQUAL(lsp::rpc::read_message(stream).value(), "hello");
    REQUIRE_EQUAL(lsp::rpc::read_message(stream).value(), "world!");

    REQUIRE(not lsp::rpc::read_message(stream).has_value());
}

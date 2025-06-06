#include <libutl/utilities.hpp>
#include <libutl/mailbox.hpp>
#include <cppunittest/unittest.hpp>

using namespace ki;

UNITTEST("mailbox")
{
    utl::Mailbox<std::string> mailbox;

    REQUIRE(mailbox.is_empty());
    REQUIRE(mailbox.pop() == std::nullopt);

    mailbox.push(5, 'a');

    REQUIRE(not mailbox.is_empty());
    REQUIRE(mailbox.pop() == "aaaaa");
    REQUIRE(mailbox.pop() == std::nullopt);

    mailbox.push("aaa");
    mailbox.push("bbb");
    mailbox.push("ccc");

    REQUIRE(not mailbox.is_empty());
    REQUIRE(mailbox.pop() == "aaa");
    REQUIRE(mailbox.pop() == "bbb");
    REQUIRE(mailbox.pop() == "ccc");
    REQUIRE(mailbox.pop() == std::nullopt);
    REQUIRE(mailbox.is_empty());
}

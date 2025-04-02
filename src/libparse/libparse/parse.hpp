#ifndef KIELI_LIBPARSE_PARSE
#define KIELI_LIBPARSE_PARSE

#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>

namespace ki::parse {

    auto parse(Database& db, Document_id doc_id) -> cst::Module;

} // namespace ki::parse

#endif // KIELI_LIBPARSE_PARSE

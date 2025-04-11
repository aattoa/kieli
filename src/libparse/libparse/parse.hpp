#ifndef KIELI_LIBPARSE_PARSE
#define KIELI_LIBPARSE_PARSE

#include <libcompiler/db.hpp>

namespace ki::par {

    auto parse(db::Database& db, db::Document_id doc_id) -> cst::Module;

} // namespace ki::par

#endif // KIELI_LIBPARSE_PARSE

#ifndef KIELI_LIBDISPLAY_DISPLAY
#define KIELI_LIBDISPLAY_DISPLAY

#include <libcompiler/db.hpp>

namespace ki::dis {

    // Parse and desugar the given document and display its AST in a tree format.
    void display_document(std::ostream& stream, db::Database& db, db::Document_id doc_id);

} // namespace ki::dis

#endif // KIELI_LIBDISPLAY_DISPLAY

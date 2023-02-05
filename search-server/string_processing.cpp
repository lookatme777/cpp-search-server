#include "string_processing.h"


std::ostream& operator << (std::ostream& out, const Document search) {
    return out << "{ document_id = " << search.id << ", relevance = " << search.relevance << ", rating = " << search.rating << " }";
}

#include "string_processing.h"


std::ostream& operator << (std::ostream& out, const Document search) {
    return out << "{ document_id = " << search.id << ", relevance = " << search.relevance << ", rating = " << search.rating << " }";
}

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

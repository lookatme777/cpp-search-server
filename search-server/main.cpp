#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <optional>
#include <utility>
#include <vector>
#include <numeric>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double VALUE = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
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

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

bool CheckMinus(const string& word) {
    if (word[word.size() - 1] == '-')
        return false;
    else {
        for (size_t i = 0; i < word.size() - 1; ++i) {
            if (word[i] == '-' && word[i + 1] == '-')
                return false;
        }
    }
    return true;
}


class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(StringContainer stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        for (string word : stop_words)
        {
            if (!IsValidWord(word)) {
                stop_words.clear();
                throw invalid_argument("Error: invalid_argument(SearchServerConstruct1)");
            }
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {  // Invoke delegating constructor from string container
        if (!IsValidWord(stop_words_text)) {
            throw invalid_argument("Error: invalid_argument(SearchServerConstruct1)");
        }
    }


    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        for (const auto& [key, value] : documents_) {
            if ((document_id == key) || (INVALID_DOCUMENT_ID >= document_id) || !IsValidWord(document))
                throw invalid_argument("invalid_argument(AddDocument)");
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }
    

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        if (CheckMinus(raw_query) && IsValidWord(raw_query))
        {
            const Query query = ParseQuery(raw_query);
            auto matched_documents = FindAllDocuments(query, document_predicate);

            sort(matched_documents.begin(), matched_documents.end(),
                [](const Document& lhs, const Document& rhs) {
                    if (abs(lhs.relevance - rhs.relevance) < VALUE) {
                        return lhs.rating > rhs.rating;
                    }
                    return lhs.relevance > rhs.relevance;
                    
                });
            if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
                matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
            }
            return matched_documents;
        }
        throw invalid_argument("invalid_argument(FindDocuments)");
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        if (CheckMinus(raw_query) && IsValidWord(raw_query))
        {
            const Query query = ParseQuery(raw_query);
            vector<string> matched_words;
            for (const string& word : query.plus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.push_back(word);
                }
            }
            for (const string& word : query.minus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.clear();
                    break;
                }
            }
            return tuple{ matched_words, documents_.at(document_id).status };
        }
        throw invalid_argument(" invalid_argument(MatchDocument)");
    }



    int GetDocumentId(int index) const {
        if (index > GetDocumentCount() || index <= INVALID_DOCUMENT_ID)
            throw out_of_range(" out_of_range(GetDocumentID");
        else
            return index;
    }

private:

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;


    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    //проверка на спецсимволы
    static bool IsValidWord(const string& word)
    {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        /*for (const int rating : ratings) {
            rating_sum += rating;
        }*/
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}
int main() {

    setlocale(LC_ALL, "RU");

    try
    {

        SearchServer search_server("и в на"s);
        (void)search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        (void)search_server.AddDocument(2, "ewaeae awea "s, DocumentStatus::ACTUAL, { 1, 2 });
        (void)search_server.AddDocument(3, "изумительный playboy"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        (void)search_server.AddDocument(4, "большой пёс скворец"s, DocumentStatus::ACTUAL, { 1, 3, 2, 4 });

        // Явно игнорируем результат метода AddDocument, чтобы избежать предупреждения
        // о неиспользуемом результате его вызова
        (void)search_server.AddDocument(5, "пушистый кот пушистый хвостcc"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

        vector<Document> documents = search_server.FindTopDocuments("пушистый"s);
        for (const Document& document : documents) {
            PrintDocument(document);
        }

    }
    catch (const invalid_argument& e) {
        cout << "Error: "s << e.what() << endl;
    }
}

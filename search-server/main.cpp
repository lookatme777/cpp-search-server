#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus doc_status, int rating) { return doc_status == status; });
    }


    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
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
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
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
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
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

    template<typename Docpredicate>
    vector<Document> FindAllDocuments(const Query& query, Docpredicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
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
        << "rating = "s << document.rating
        << " }"s << endl;
}
int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "ACTUAL:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}

//ТЕСТЫ
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestMinusWordsInQuery() {

    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    const int doc_id_ = 43;
    const string content_ = "green cat from gold city"s;
    const vector<int> ratings_ = {3, 2, 1};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_, content_, DocumentStatus::ACTUAL, ratings_);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.size() == 2);
        const auto found_docs_ = server.FindTopDocuments("cat -from"s); // добавили минус-слово
        ASSERT(found_docs_.size() == 1);    // из результата поиска ушел документ с таким словом
    }
}

void TestDocumentsMatching() {

    const int doc_id_ = 43;
    const string content_ = "green cat from gold city"s;
    const vector<int> ratings_ = {3, 2, 1};

    const string raw_query_ = "from green gold"s;
    const string raw_query_1 = "from -green gold"s;
    {
        SearchServer server;
        server.AddDocument(doc_id_, content_, DocumentStatus::ACTUAL, ratings_);

        tuple<vector<string>, DocumentStatus> matched = server.MatchDocument(raw_query_, doc_id_);
        ASSERT( (get<0>(matched)).size() == 3);   

        matched = server.MatchDocument(raw_query_1, doc_id_);
        ASSERT((get<0>(matched)).size() == 0);      
    }
}

void Test_RatingCalc_TFIDFCalc_SortDocsByRelevance() {
    const int doc_id = 42;
    const string content = "cat in the big city"s;
    const vector<int> ratings = {1, 2, 3};

    const int doc_id_1 = 43;
    const string content_1 = "green cat from gold city"s;
    const vector<int> ratings_1 = {4, 5, 6};

    const int doc_id_2 = 44;
    const string content_2 = "angry cat from outer space"s;
    const vector<int> ratings_2 = {2, 3, 4};

    const string raw_query = "angry from cat"s;
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        // результат поиска
        vector<Document> found_docs = server.FindTopDocuments(raw_query);

        ASSERT(found_docs[0].id == 44 && found_docs[1].id == 43 && found_docs[2].id == 42);
        ASSERT(found_docs[0].rating == 3 && found_docs[1].rating == 5 && found_docs[2].rating == 2);
        ASSERT(found_docs[0].relevance > found_docs[1].relevance && found_docs[1].relevance > found_docs[2].relevance);

      
        double idf_angry = log(3.0 / 1);
        double idf_from = log(3.0 / 2);
        double idf_cat = log(3.0 / 3);

        double tf_angry_in_id42 = 0.0/5,  tf_from_in_id42 = 0.0/5,    tf_cat_in_id42 = 1.0/5;
        double tf_angry_in_id43 = 0.0/5,  tf_from_in_id43 = 1.0/5,    tf_cat_in_id43 = 1.0/5;
        double tf_angry_in_id44 = 1.0/5,  tf_from_in_id44 = 1.0/5,    tf_cat_in_id44 = 1.0/5;

        // перемножим, сложим, посолим ...
        double relevance_id42 = idf_angry * tf_angry_in_id42 +
                                idf_from * tf_from_in_id42 +
                                idf_cat * tf_cat_in_id42;

        double relevance_id43 = idf_angry * tf_angry_in_id43 +
                                idf_from * tf_from_in_id43 +
                                idf_cat * tf_cat_in_id43;

        double relevance_id44 = idf_angry * tf_angry_in_id44 +
                                idf_from * tf_from_in_id44 +
                                idf_cat * tf_cat_in_id44;
   
        ASSERT(relevance_id44 == found_docs[0].relevance);
        ASSERT(relevance_id43 == found_docs[1].relevance);
        ASSERT(relevance_id42 == found_docs[2].relevance);
      
    }
}

void TestSearchByStatus() {
    {
        SearchServer server;
        const auto found_docs1 = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs1.size() == 0);
        const auto found_docs2 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT(found_docs2.size() == 0);
        const auto found_docs3 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT(found_docs3.size() == 0);
        const auto found_docs4 = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT(found_docs4.size() == 0);

   
        server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {1,2});
        server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, {1, 2});
        server.AddDocument(2, "cat"s, DocumentStatus::ACTUAL, {1, 2});
        server.AddDocument(3, "cat"s, DocumentStatus::ACTUAL, {1, 2});
        server.AddDocument(4, "cat"s, DocumentStatus::ACTUAL, {1, 2});

        const auto found_docs5 = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs5.size() == 5);                                                        
        const auto found_docs6 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT(found_docs6.size() == 0);
        const auto found_docs7 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT(found_docs7.size() == 0);
        const auto found_docs8 = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT(found_docs8.size() == 0);

     
        server.AddDocument(5, "cat"s, DocumentStatus::IRRELEVANT, {1, 2});
        server.AddDocument(6, "cat"s, DocumentStatus::IRRELEVANT, {1, 2});
        server.AddDocument(7, "cat"s, DocumentStatus::IRRELEVANT, {1, 2});
        server.AddDocument(8, "cat"s, DocumentStatus::IRRELEVANT, {1, 2});

        const auto found_docs9 = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs9.size() == 5);
        const auto found_docs10 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT(found_docs10.size() == 4);                                                       
        const auto found_docs11 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT(found_docs11.size() == 0);
        const auto found_docs12 = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT(found_docs12.size() == 0);

     
        server.AddDocument(9, "cat"s, DocumentStatus::BANNED, {1, 2});
        server.AddDocument(10, "cat"s, DocumentStatus::BANNED, {1, 2});
        server.AddDocument(11, "cat"s, DocumentStatus::BANNED, {1, 2});

        const auto found_docs13 =server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs13.size() == 5);
        const auto found_docs14 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT(found_docs14.size() == 4);
        const auto found_docs15 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT(found_docs15.size() == 3);                                                      
        const auto found_docs16 = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT(found_docs16.size() == 0);

       
        server.AddDocument(12, "cat"s, DocumentStatus::REMOVED, {1, 2});
        server.AddDocument(13, "cat"s, DocumentStatus::REMOVED, {1, 2});

        const auto found_docs17 = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs17.size() == 5);
        const auto found_docs18 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT(found_docs18.size() == 4);
        const auto found_docs19 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT(found_docs19.size() == 3);
        const auto found_docs20 = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT(found_docs20.size() == 2);                                                     
    }
}

void TestFilteringByPredicat() {
    {
        SearchServer server;
        server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {1, 2, 3});   
        server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, {2, 3, 4});   
        server.AddDocument(2, "cat"s, DocumentStatus::ACTUAL, {3, 4, 5});   

        server.AddDocument(4, "cat"s, DocumentStatus::IRRELEVANT, {1, 2, 3});  
        server.AddDocument(5, "cat"s, DocumentStatus::IRRELEVANT, {2, 3, 4});  
        server.AddDocument(6, "cat"s, DocumentStatus::IRRELEVANT, {3, 4, 5});  

        server.AddDocument(8, "cat"s, DocumentStatus::BANNED, {1, 2, 3});  
        server.AddDocument(9, "cat"s, DocumentStatus::BANNED, {2, 3, 4});  
        server.AddDocument(10, "cat"s, DocumentStatus::BANNED, {3, 4, 5});  

        server.AddDocument(12, "cat"s, DocumentStatus::REMOVED, {1, 2, 3});  
        server.AddDocument(13, "cat"s, DocumentStatus::REMOVED, {2, 3, 4});  
        server.AddDocument(14, "cat"s, DocumentStatus::REMOVED, {3, 4, 5});  

        // поиск по id = 10 документа
        vector<Document> found_docs = server.FindTopDocuments("cat"s,
                                                              [](int document_id, DocumentStatus status, int rating) { return document_id == 10; });
        ASSERT(found_docs.size() == 1 && found_docs.at(0).id == 10 && found_docs.at(0).rating == 4);  

      
        found_docs = server.FindTopDocuments("cat"s,
                                             [](int document_id, DocumentStatus status, int rating) { return rating == 3; });
        ASSERT(found_docs.size() == 4);
        ASSERT(found_docs.at(0).id == 1 && found_docs.at(1).id == 5 &&  
               found_docs.at(2).id == 9 && found_docs.at(3).id == 13);

        // поиск документа по status = IRRELEVANT
        found_docs = server.FindTopDocuments(
                "cat"s, [](int document_id, DocumentStatus status, int rating) {return status == DocumentStatus::IRRELEVANT; });
        ASSERT(found_docs.size() == 3);
        ASSERT(found_docs.at(0).id == 6 && found_docs.at(1).id == 5 && found_docs.at(2).id == 4); 
    }
}

void Test1() {
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWordsInQuery);
    RUN_TEST(TestDocumentsMatching);
    RUN_TEST(Test_RatingCalc_TFIDFCalc_SortDocsByRelevance);
    RUN_TEST(TestSearchByStatus);
    RUN_TEST(TestFilteringByPredicat);
    // Не забудьте вызывать остальные тесты здесь
}

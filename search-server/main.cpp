#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <map>
#include <vector>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
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
};

class SearchServer {
public:

    void SetStopWords(const string &text) {
        for (const string &word: SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document) {
        ++document_count_;
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double TF= 1.0/words.size();

        for (const auto &word: words) {

            word_to_document_freqs_[word][document_id]+= TF;

        }

    }

    vector<Document> FindTopDocuments(const string &raw_query) const {
        const Query query_words = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<string> m_words;
        set<string> p_words;
    };

    map<string, map<int,double>> word_to_document_freqs_;
    int document_count_ = 0;
    set<string> stop_words_;

    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const {
        vector<string> words;
        for (const auto &word: SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string &text) const {
        Query query;
        for (const string &word: SplitIntoWordsNoStop(text)) {
            if (word[0] == '-')
                query.m_words.insert(word.substr(1));
            else
                query.p_words.insert(word);
        }
        return query;
    }

    double IDF(const string& word) const{
        return log(document_count_ * 1.0/word_to_document_freqs_.at(word).size());
    }

    vector<Document> FindAllDocuments(const Query &query_words) const {
        vector<Document> matched_documents;
        map<int, double> document_to_relevance;
        for (const auto &word: query_words.p_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double idf = IDF(word);
            for (const auto &[id, tf]: word_to_document_freqs_.at(word)) {
                document_to_relevance[id] += tf*idf;
            }
        }
        for (const auto& mid : query_words.m_words){
            if(word_to_document_freqs_.count(mid) == 0){
                continue;
            }
            for(const auto& [id, value] : word_to_document_freqs_.at(mid)){
                document_to_relevance.erase(value);
            }

        }
        for (const auto &[key, value]: document_to_relevance) {
            matched_documents.push_back({key, value});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main(){
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", " << "relevance = "s << relevance << " }"s << endl;
    }
}

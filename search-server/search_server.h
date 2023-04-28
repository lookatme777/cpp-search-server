#pragma once
#include <iostream>
#include <algorithm>
#include <execution>
#include <map>
#include <string_view>
#include <cmath>
#include <vector>
#include <deque>
#include <functional>
#include "document.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include "concurrent_map.h"


const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double VALUE = 1e-6;
const unsigned int NUMTHREAD = std::thread::hardware_concurrency();

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const string_view stop_words_text);

    explicit SearchServer(const string& stop_words_text);

    void AddDocument(int document_id, const string_view& document, DocumentStatus status,
        const vector<int>& ratings);

    template <typename DocumentPredicate, typename Policy>
    std::vector<Document> FindTopDocuments(Policy&& policy, string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename Policy>
    std::vector<Document> FindTopDocuments(Policy&& policy, string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(string_view raw_query, DocumentStatus status) const;

    template <typename Policy>
    std::vector<Document> FindTopDocuments(Policy&& policy, string_view raw_query) const;
    std::vector<Document> FindTopDocuments(string_view raw_query) const;

    int GetDocumentCount() const;

    set<int>::const_iterator begin() const;
    set<int>::const_iterator end() const;
    set<int>::iterator begin();
    set<int>::iterator end();

    tuple<vector<string_view>, DocumentStatus> MatchDocument(string_view raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy ex, string_view raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy ex, string_view raw_query, int document_id) const;
    const map<string_view, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::parallel_policy policy, int document_id);
    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);

    bool IsStopWord(const string_view word) const;

private:
    deque<string>word_;
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    std::set<std::string, std::less<>> stop_words_;
    map<string_view, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    set<int> document_ids_;
    map<int, map<string_view, double>>word_freqs;



    static bool IsValidWord(const string_view word);

    vector<string_view> SplitIntoWordsNoStop(const string_view text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWord {
        string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string_view text) const;

    struct Query {
        vector<string_view> plus_words;
        vector<string_view> minus_words;
    };

    Query ParseQuery(string_view text) const;
    Query ParseQuery(bool flag, string_view text) const;

    double ComputeWordInverseDocumentFreq(const string_view& word) const;


    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    vector<Document> FindAllDocuments(const execution::sequenced_policy& policy, string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    vector<Document> FindAllDocuments(const execution::parallel_policy& policy, string_view raw_query, DocumentPredicate document_predicate) const;

};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw invalid_argument("words are invalid"s);
    }
}


template <typename DocumentPredicate, typename Policy>
vector<Document> SearchServer::FindTopDocuments(Policy&& policy, string_view raw_query, DocumentPredicate document_predicate) const {
    auto matched_documents = FindAllDocuments(policy, raw_query, document_predicate);

    std::sort(policy,
        matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < VALUE) {
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

template <typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename Policy>
vector<Document> SearchServer::FindTopDocuments(Policy&& policy, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,
        [&status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <typename Policy>
vector<Document> SearchServer::FindTopDocuments(Policy&& policy, string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(string_view raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    std::map<int, double> document_to_relevance;

    for (auto word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const DocumentData& documents_data = documents_.at(document_id);
            if (document_predicate(document_id, documents_data.status, documents_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (auto word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(const execution::sequenced_policy& policy, string_view raw_query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    const auto query = ParseQuery(raw_query);

    for (auto word : query.plus_words) {
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

    for (auto word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(const execution::parallel_policy& policy, string_view raw_query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(NUMTHREAD);
    const auto query = ParseQuery(raw_query);

    std::for_each(policy,
        query.minus_words.begin(), query.minus_words.end(),
        [this, &document_to_relevance](std::string_view word) {
            if (word_to_document_freqs_.count(word)) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.dell(document_id);
                }
            }
        });

    for_each(policy,
        query.plus_words.begin(), query.plus_words.end(),
        [this, &document_predicate, &document_to_relevance](string_view word) {
            if (word_to_document_freqs_.count(word)) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });

    std::map<int, double> document_to_relevance_next = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance_next.size());

    for (const auto [document_id, relevance] : document_to_relevance_next) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

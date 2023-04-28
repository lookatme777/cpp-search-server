#include "search_server.h"

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(
        SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(
        SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status,
    const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    word_.push_back(std::string(document));
    const auto words = SplitIntoWordsNoStop(std::string_view(word_.back()));



    const double inv_word_count = 1.0 / words.size();
    for (string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}


vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}



int SearchServer::GetDocumentCount() const {
    return documents_.size();
}


set<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

set<int>::iterator SearchServer::begin()
{
    return document_ids_.begin();
}

set<int>::iterator SearchServer::end()
{
    return document_ids_.end();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { matched_words, documents_.at(document_id).status };
        }
    }
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}


tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy ex, string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy policy, std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(true, raw_query);
    std::vector<std::string_view> matched_words;
    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), [&](auto word) {
        if (word_to_document_freqs_.count(word)) {
            if (word_to_document_freqs_.at(word).count(document_id)) {
                return true;
            }
        }
    return false;
        }) == true) return { matched_words, documents_.at(document_id).status };

        matched_words.resize(query.plus_words.size());
        auto end = copy_if(policy, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](auto word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return false;
            }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return true;
        }
        return false;
            });

        sort(matched_words.begin(), end);
        auto it = unique(matched_words.begin(), end);
        matched_words.erase(it, matched_words.end());
        return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for (const string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }

    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Query word "s + string(text) + " is invalid");
    }

    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query result;
    for (string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    sort(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(unique(result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());
    sort(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(unique(result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());

    return result;
}

SearchServer::Query SearchServer::ParseQuery(bool flag, string_view text) const {
    Query result;
    for (string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view& word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {

    static map<string_view, double>nullmap{};

    if (word_freqs.count(document_id) == 1) {
        return word_freqs.at(document_id);
    }
    else
        return nullmap;
}

void SearchServer::RemoveDocument(int document_id)
{
    if (word_freqs.count(document_id) == 0) {
        return;
    }

    for (auto& [word, freq] : word_freqs.at(document_id)) {

        word_to_document_freqs_.at(word).erase(document_id);
    }

    document_ids_.erase(document_id);
    documents_.erase(document_id);
    word_freqs.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy policy, int document_id)
{
    if (word_freqs.count(document_id) == 0) {
        return;
    }

    std::for_each(policy, word_freqs.at(document_id).begin(), word_freqs.at(document_id).end(), [&document_id, this](auto& wordAndData)
        {
            word_to_document_freqs_.at(wordAndData.first).erase(document_id);
        });

    document_ids_.erase(document_id);
    documents_.erase(document_id);
    word_freqs.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy policy, int document_id)
{
    if (word_freqs.count(document_id) == 0) {
        return;
    }
    std::vector<std::string_view> vec(word_freqs.at(document_id).size());
    std::transform(policy, word_freqs.at(document_id).begin(), word_freqs.at(document_id).end(), vec.begin(), [](auto& a)
        {
            return a.first;
        });

    std::for_each(policy, vec.begin(), vec.end(), [&document_id, this](auto& word)
        {
            word_to_document_freqs_.at(word).erase(document_id);
        });

    document_ids_.erase(document_id);
    documents_.erase(document_id);
    word_freqs.erase(document_id);
}

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <execution>
#include <deque>
#include <utility>
#include "log_duration.h"
#include "search_server.h"

using namespace std;

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id");
    }
    set<string> words_set;
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        doc_to_word_freqs_[document_id][word] += inv_word_count;
        word_to_document_freqs_[word][document_id] += inv_word_count;
        words_set.insert(word);
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });

    document_ids_.insert(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;
    for (const std::string& word : query.plus_words) {
        auto it = doc_to_word_freqs_.at(document_id).find(word);
        if (it == doc_to_word_freqs_.at(document_id).end()) {
            continue;
        }
        else {
            matched_words.push_back(it->first);
        }
    }
    for (const std::string& word : query.minus_words) {
        auto it = doc_to_word_freqs_.at(document_id).find(word);
        if (it == doc_to_word_freqs_.at(document_id).end()) {
            continue;
        }
        else {
            matched_words.clear();
            break;
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const {
    return SearchServer::document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return SearchServer::document_ids_.end();
}

const map<std::string_view, double> SearchServer::GetWordFrequencies(int document_id) const {
    static const map<std::string_view, double> empty_map{};
    if (doc_to_word_freqs_.find(document_id) == doc_to_word_freqs_.end()) {
        return empty_map;
    }
    std::map<std::string_view, double> map_stringview;
    for (auto& word_freq : doc_to_word_freqs_.at(document_id)) {
        map_stringview[word_freq.first] = word_freq.second;
    }

    return map_stringview;
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.find(document_id) != document_ids_.end()) {
        for (auto word : doc_to_word_freqs_.at(document_id)) {
            word_to_document_freqs_.erase(word.first);
        }
        doc_to_word_freqs_.erase(document_id);
        documents_.erase(document_id);
        document_ids_.erase(document_id);
    }
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    vector<string> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word " + string(word) + " is invalid");
        }
        if (!IsStopWord(string(word))) {
            words.push_back(string(word));
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

SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty");
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word " + text + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}
// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    SearchServer::Query result;

    for (const auto& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(std::string(word));
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }

    return result;
}

void AddDocument(SearchServer& search_server, int document_id, std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }

    catch (const std::exception& e) {
        std::cout << "Error adding document" << document_id << ": " << e.what() << std::endl;
    }
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query);
}

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query) {
    std::cout << "Search Results: " << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }

    catch (const std::exception& e) {
        std::cout << "Search error: " << e.what() << std::endl;
    }
}

void MatchDocuments(const SearchServer& search_server, std::string_view query) {
    try {
        std::cout << "Matching documents to query: " << query << std::endl;

        for (int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(std::execution::seq,query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }

    catch (const std::exception& e) {
        std::cout << "Error matching documents for query " << query << ": " << e.what() << std::endl;
    }
}

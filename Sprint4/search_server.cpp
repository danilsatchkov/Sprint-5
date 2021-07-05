#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "log_duration.h"
#include "search_server.h"

using namespace std;

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id");
    }
    set<string> words_set;
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        doc_to_word_freqs_[document_id][word]+= inv_word_count;
        word_to_document_freqs_[word][document_id] += inv_word_count;
        words_set.insert(word);
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });

    document_ids_.insert(document_id);
    
    if (words_to_doc_.find(words_set) == words_to_doc_.end()|| document_id < words_to_doc_[words_set]) {
        words_to_doc_[words_set] = document_id;
    }
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const{
    return SearchServer::document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return SearchServer::document_ids_.end();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double>& empty_map {};
    if (doc_to_word_freqs_.find(document_id) == doc_to_word_freqs_.end()) {
        return empty_map;
    }
    
    return doc_to_word_freqs_.at(document_id);
};

void SearchServer::RemoveDocument(int document_id) {
    auto it = words_to_doc_.begin();
    while (it != words_to_doc_.end())
    {
        if (it->second == document_id)
        {
            it = words_to_doc_.erase(it);
        }
        else
        {
            it++;
        }
    }
    doc_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (doc_to_word_freqs_.at(document_id).count(word) == 0) {
            continue;
        }
        if (doc_to_word_freqs_.at(document_id).count(word)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (doc_to_word_freqs_.at(document_id).count(word) == 0) {
            continue;
        }
        if (doc_to_word_freqs_.at(document_id).count(word)) {
            matched_words.clear();
            break;
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word " + word + " is invalid");
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

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    SearchServer::Query result;
    for (const string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
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

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }

    catch (const std::exception& e) {
        std::cout << "Error adding document" << document_id << ": " << e.what() << std::endl;
    }
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

void MatchDocuments(const SearchServer& search_server, const std::string& query) {
    try {
        std::cout << "Matching documents to query: " << query << std::endl;

        for (int document_id:search_server) {
            const auto[words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }

    catch (const std::exception& e) {
        std::cout << "Error matching documents for query " << query << ": " << e.what() << std::endl;
    }
}



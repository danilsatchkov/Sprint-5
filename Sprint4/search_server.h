#pragma once
#include <algorithm>
#include<execution>
#include <future>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "concurrent_map.h"
#include "string_processing.h"
#include "document.h"
#include "log_duration.h"

const int kMaxResultDocumentCount = 5;

class SearchServer {
public:
    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
    }
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate, typename Execution>
    std::vector<Document> FindTopDocuments(Execution&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename Execution>
    std::vector<Document> FindTopDocuments(Execution&& policy, std::string_view raw_query, DocumentStatus status) const;
    template <typename Execution>
    std::vector<Document> FindTopDocuments(Execution&& policy, std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;    
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;   
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    int GetDocumentCount() const;

    template<typename Execution>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(Execution&& policy, std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double> GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    template <typename Execution>
    void RemoveDocument(Execution&& policy, int document_id);
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string, double>> doc_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string& word) const;
    static bool IsValidWord(std::string_view word);
    std::vector<std::string> SplitIntoWordsNoStop(std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string& text) const;

    struct Query {
        std::set<std::string, std::less<>> plus_words;
        std::set<std::string, std::less<>> minus_words;       
    };

    
    Query ParseQuery(std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename K, typename V, typename Execution>
    std::map<K, V> MaptovecRemove(Execution&& policy, std::map<K, V>& input_map, int document_id);
    
    template <typename DocumentPredicate,typename Execution>
    std::vector<Document> FindAllDocuments(Execution&& policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename DocumentPredicate, typename Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {

    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > kMaxResultDocumentCount) {
        matched_documents.resize(kMaxResultDocumentCount);
    }

    return matched_documents;
}

template <typename Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution&& policy, std::string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

template <typename Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution&& policy, std::string_view raw_query) const {
    return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename Execution>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(Execution&& policy,std::string_view raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;

    for_each(query.plus_words.begin(), query.plus_words.end(), [&matched_words, this, document_id] 
        (auto const& word) {

        auto it = doc_to_word_freqs_.at(document_id).find(word);
        if (it != doc_to_word_freqs_.at(document_id).end()) {
            matched_words.push_back(it->first);
        }   
        });

    for_each(query.minus_words.begin(), query.minus_words.end(), [&matched_words, this, document_id]
    (auto const& word) {
            auto it = doc_to_word_freqs_.at(document_id).find(word);
            if (it != doc_to_word_freqs_.at(document_id).end()) {
                matched_words.clear();
            }
           });

    return { matched_words, documents_.at(document_id).status };
}

template <typename K, typename V, typename Execution>
std::map<K, V> MaptovecRemove(Execution&& policy, std::map<K, V>& input_map, int document_id) {
    std::vector<std::pair<K, V>> output_vec(make_move_iterator(input_map.begin()), make_move_iterator(input_map.end()));

    std::remove_if(policy, output_vec.begin(), output_vec.end(), [document_id](auto doc) {
        return doc.first == document_id;
        });

    std::map<K, V> output_map(make_move_iterator(output_vec.begin()), make_move_iterator(output_vec.end()));
    return output_map;
}

template <typename DocumentPredicate,typename Execution>
std::vector<Document> SearchServer::FindAllDocuments(Execution&& policy,const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(16);

    for_each(policy, doc_to_word_freqs_.begin(), doc_to_word_freqs_.end(),
        [this,document_predicate, &query, &document_to_relevance] (const auto& doc) {
            auto document_id = doc.first;
            auto word_freq = doc.second;

            const auto& document_data = documents_.at(document_id);
            for (const std::string& word : query.plus_words) {
                //check if plus word is contained in the document
                if (word_freq.find(word) == word_freq.end()) {
                    continue;
                }
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += word_freq.at(word) * inverse_document_freq;
                }
            }

            for (const std::string& word : query.minus_words) {
                if (word_freq.find(word) == word_freq.end()) {
                    continue;
                }
                document_to_relevance.Erase(document_id);
            }
        });

    auto document_to_relevance_ord = document_to_relevance.BuildOrdinaryMap();

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance_ord) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename Execution>
void SearchServer::RemoveDocument(Execution&& policy, int document_id) {
    if (document_ids_.find(document_id) != document_ids_.end()) {
        std::for_each(policy, word_to_document_freqs_.begin(), word_to_document_freqs_.end(), [document_id](auto& word) {
            word.second.erase(document_id);

            });

        doc_to_word_freqs_ = MaptovecRemove(policy, doc_to_word_freqs_, document_id);
        documents_ = MaptovecRemove(policy, documents_, document_id);
        document_ids_.erase(document_id);
    }
}
void AddDocument(SearchServer& search_server, int document_id, std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);
void MatchDocuments(const SearchServer& search_server, std::string_view query);
std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries);
std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries);
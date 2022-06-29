#pragma once
#include <string>
#include <vector>
#include <utility>
#include <set>
#include <map>
#include <execution>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <string_view>
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAX_DELTA_ERROR = 1e-6;
const int THREAD_COUNT = 4;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(string_view stop_words_text);
    explicit SearchServer(const string& stop_words_text);
    explicit SearchServer(const char* stop_words_text);

    template <typename CharContainer>
void AddDocument(int document_id, const CharContainer& document, DocumentStatus status,
                               const vector<int>& ratings);

template <typename CharContainer, typename DocumentPredicate>
vector<Document> FindTopDocuments(const CharContainer& raw_query,
                                  DocumentPredicate document_predicate) const;
    template <typename CharContainer>
    vector<Document> FindTopDocuments(const CharContainer& raw_query,
                                      DocumentStatus status) const;
    template <typename CharContainer>
    vector<Document> FindTopDocuments(const CharContainer& raw_query) const;
    template <typename CharContainer, typename DocumentPredicate>
vector<Document> FindTopDocuments(std::execution::parallel_policy policy,
                                  const CharContainer& raw_query,
                                  DocumentPredicate document_predicate) const;
    template <typename CharContainer>
vector<Document> FindTopDocuments(std::execution::parallel_policy policy,
                                  const CharContainer& raw_query,
                                  DocumentStatus status) const;
    template <typename CharContainer>
vector<Document> FindTopDocuments(std::execution::parallel_policy policy,
                                  const CharContainer& raw_query) const;
    template <typename CharContainer, typename DocumentPredicate>
vector<Document> FindTopDocuments(std::execution::sequenced_policy policy,
                                  const CharContainer& raw_query,
                                  DocumentPredicate document_predicate) const;
    template <typename CharContainer>
vector<Document> FindTopDocuments(std::execution::sequenced_policy policy,
                                  const CharContainer& raw_query,
                                  DocumentStatus status) const;
    template <typename CharContainer>
vector<Document> FindTopDocuments(std::execution::sequenced_policy policy,
                                  const CharContainer& raw_query) const;

    int GetDocumentCount() const;

    set<int>::const_iterator begin() const;
    set<int>::const_iterator end() const;

    template <typename CharContainer>
    tuple<vector<string_view>, DocumentStatus> MatchDocument(const CharContainer& raw_query,
                                                             int document_id) const;
    template <typename CharContainer>
    tuple<vector<string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy policy,
                                                             const CharContainer& raw_query,
                                                             int document_id) const;
    template <typename CharContainer>
    tuple<vector<string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy policy,
                                                             const CharContainer& raw_query,
                                                             int document_id) const;

    const map<string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    template<typename ExecPolicy>
    void RemoveDocument(ExecPolicy&& policy, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    struct Query {
        vector<string_view> plus_words;
        vector<string_view> minus_words;
        
        void MakeUnique(); // Makes query fields sorted and unique just like set;
    };
    
    const set<string, less<>> stop_words_;
    map<string_view, map<int, double>> word_to_document_freqs_;
    map<int, map<string_view, double>> word_freqs_by_id_;
    map<int, DocumentData> documents_;
    set<int> document_ids_;
    set<string, less<>> all_words_;

    bool IsStopWord(string_view word) const;

    static bool IsValidWord(string_view word);

    vector<string_view> SplitIntoWordsNoStop(string_view text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWord {
        string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string_view word) const;

    template <typename CharContainer>
    Query ParseQuery(const CharContainer& text) const;
    
    // Existence required
    double ComputeWordInverseDocumentFreq(string_view word) const;

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(std::execution::parallel_policy policy,
                                      const Query& query,
                                      DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(std::execution::sequenced_policy policy,
                                      const Query& query,
                                      DocumentPredicate document_predicate) const;
    
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(),
                [](auto& i){return SearchServer::IsValidWord(i);})) {
        throw invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename CharContainer, typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(const CharContainer& raw_query,
                                                DocumentPredicate document_predicate) const {

    auto query = ParseQuery(raw_query);
    query.MakeUnique();

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < MAX_DELTA_ERROR) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
   
    return matched_documents;
}


template <typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(const Query& query,
                                                DocumentPredicate document_predicate) const {

    map<int, double> document_to_relevance;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        
        const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
        
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    
    return matched_documents;
}

template<typename ExecPolicy>
void SearchServer::RemoveDocument(ExecPolicy&& policy, int document_id) {
    
    if (word_freqs_by_id_.count(document_id)) {
        
        vector<string_view> tmp(word_freqs_by_id_.at(document_id).size());
        
        for (const auto i : word_freqs_by_id_[document_id]) {
            tmp.push_back(i.first);
        }

        for_each(policy,
        tmp.begin(), tmp.end(),
        [document_id, this ](auto str){ 
            if (this->word_to_document_freqs_.count(*str)) {
                this->word_to_document_freqs_[str].erase(document_id);
            }
        }); 
        
        for (auto i : tmp) {
            if (word_to_document_freqs_.at(i).empty()) {
                this->word_to_document_freqs_.erase(i);
            }
        }
            
        word_freqs_by_id_.erase(document_id);
        documents_.erase(document_id);
        document_ids_.erase(document_id);
    }
}

template <typename CharContainer>
void SearchServer::AddDocument(int document_id,
                               const CharContainer& document,
                               DocumentStatus status,
                               const vector<int>& ratings) {
    
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    
    auto words = SplitIntoWordsNoStop(static_cast<string_view>(document));
    
    const double inv_word_count = 1.0 / words.size();
    
    for (string_view word : words) {
        auto i = all_words_.insert(static_cast<string>(word));
        string_view new_str = *(i.first);
        word_to_document_freqs_[new_str][document_id] += inv_word_count;
        word_freqs_by_id_[document_id][new_str] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

template <typename CharContainer>
vector<Document> SearchServer::FindTopDocuments(const CharContainer& raw_query,
                                                DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int, DocumentStatus document_status, int) {
        return document_status == status;
    });
}

template <typename CharContainer>
vector<Document> SearchServer::FindTopDocuments(const CharContainer& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

template <typename CharContainer>
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const CharContainer& raw_query,
                                                                       int document_id) const {
    
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("MatchDocument: out_of_range");
    }
    
    string_view to_str_query = static_cast<string_view>(raw_query);
    
    if (to_str_query.empty()) {
        throw std::invalid_argument("MatchDocument: invalid_argument");
    }
    
    auto query = ParseQuery(to_str_query);
    query.MakeUnique();
    
    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {vector<string_view>(), documents_.at(document_id).status};
        }
    }
    
    vector<string_view> matched_words;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    sort(matched_words.begin(), matched_words.end());
    auto it = unique(matched_words.begin(), matched_words.end());
    matched_words.resize(distance(matched_words.begin(), it));
    
    return {matched_words, documents_.at(document_id).status};
}

template <typename CharContainer>
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy policy,
                                                                       const CharContainer& raw_query,
                                                                       int document_id) const {
    
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("MatchDocument: out_of_range");
    }
    
    string_view to_str_query = static_cast<string_view>(raw_query);
    
    if (to_str_query.empty()) {
        throw std::invalid_argument("MatchDocument: invalid_argument");
    }

    vector<string_view> result;

    Query query = ParseQuery(to_str_query);

    /*  --- PARALLEL VERSION IS SLOWER ---
    int to_delete = transform_reduce(policy,
    query.minus_words.begin(), query.minus_words.end(),
    0,
    plus<>{},                                 
    [document_id, this](const auto& word){
        if (this->word_to_document_freqs_.count(word) == 0) {
            return 0;
        }
        if (this->word_to_document_freqs_.at(word).count(document_id) > 0) {
            return 1;
        }
        return 0;
    });
    
    if (to_delete != 0) {
        return {vector<string_view>(), documents_.at(document_id).status};
    }*/
    
    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {vector<string_view>(), documents_.at(document_id).status};
        }
    }
    
    const string_view null_str = " ";
    vector<string_view> matched_words(query.plus_words.size(), null_str);

    transform(policy,
    query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
    [null_str, document_id, this](auto& word){
        if (this->word_to_document_freqs_.count(word) == 0) {
            return null_str;
        }
        if (this->word_to_document_freqs_.at(word).count(document_id)) {
            return word;
        }
        return null_str;
    });
    
    for (auto i : matched_words) {
        if (i != null_str) {
            result.push_back(move(i));
        }
    }

    sort(result.begin(), result.end());
    auto it = unique(result.begin(), result.end());
    result.resize(distance(result.begin(), it));
    
    return {result, documents_.at(document_id).status};

}

template <typename CharContainer>
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy policy,
                                                                       const CharContainer& raw_query,
                                                                       int document_id) const {
    return MatchDocument(raw_query, document_id);
}

template <typename CharContainer>
SearchServer::Query SearchServer::ParseQuery(const CharContainer& text) const {
    
    Query result;
    for (string_view word : SplitIntoWordsStringView(std::string_view(text))) {
        auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

template <typename CharContainer, typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy policy,
                                                const CharContainer& raw_query,
                                                DocumentPredicate document_predicate) const {

    
    auto query = ParseQuery(raw_query);
    query.MakeUnique();

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs,
                                                                const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < MAX_DELTA_ERROR) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
   
    return matched_documents;
}

template <typename CharContainer, typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy policy,
                                                const CharContainer& raw_query,
                                                DocumentPredicate document_predicate) const {

    
    auto query = ParseQuery(raw_query);
    query.MakeUnique();

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs,
                                                                const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < MAX_DELTA_ERROR) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
   
    return matched_documents;
}

template <typename CharContainer>
vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy policy,
                                                const CharContainer& raw_query,
                                                DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int, DocumentStatus document_status, int) {
        return document_status == status;
    });
}

    template <typename CharContainer>
vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy policy,
                                                const CharContainer& raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename CharContainer>
vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy policy,
                                                const CharContainer& raw_query,
                                                DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int, DocumentStatus document_status, int) {
        return document_status == status;
    });
}

    template <typename CharContainer>
vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy policy,
                                                const CharContainer& raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy policy,
                                                const Query& query,
                                                DocumentPredicate document_predicate) const {

    ConcurrentMap<int, double> document_to_relevance(THREAD_COUNT); // Argument should be equal to max available threads;
    
    for_each(policy, query.plus_words.begin(), query.plus_words.end(), [this, 
    document_predicate, &document_to_relevance](auto& word){
        if (this->word_to_document_freqs_.count(word) != 0) {
            const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
        
            for (const auto [document_id, term_freq] : this->word_to_document_freqs_.at(word)) {
                const auto& document_data = this->documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    });

    for_each(policy, query.minus_words.begin(), query.minus_words.end(), [this, 
    document_predicate, &document_to_relevance](auto& word){
        if (this->word_to_document_freqs_.count(word) != 0) {
            for (const auto [document_id, _] : this->word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
    });

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    
    return matched_documents;
}

template <typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy policy,
                                                const Query& query,
                                                DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

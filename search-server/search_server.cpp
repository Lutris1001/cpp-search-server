#include <string>
#include <vector>
#include <cmath>
#include <set>
#include <stdexcept>
#include <numeric>
#include <functional>
#include "document.h"
#include "search_server.h"
#include "string_processing.h"


using namespace std;

SearchServer::SearchServer(string_view stop_words_text)
        : SearchServer::SearchServer(SplitIntoWordsView(stop_words_text))  // Invoke delegating constructor
// from string container
{
}

SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer::SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
// from string container
{
}

SearchServer::SearchServer(const char* stop_words_text)
        : SearchServer::SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
// from string container
{
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

// int SearchServer::GetDocumentId(int index) const {
//     return document_ids_.at(index);
// }

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

void SearchServer::RemoveDocument(int document_id) {
    
    for (const auto& [key, value] : GetWordFrequencies(document_id)) {
        word_to_document_freqs_.at(key).erase(document_id);
        if (word_to_document_freqs_.at(key).empty()) {
            word_to_document_freqs_.erase(key);
        }
    }
        word_freqs_by_id_.erase(document_id);
        documents_.erase(document_id);
        document_ids_.erase(document_id);
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) != 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> dummy_;
    if (word_freqs_by_id_.count(document_id)) {
        return word_freqs_by_id_.at(document_id);
    }
    return dummy_;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWordsStringView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + static_cast<string>(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.emplace_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view word) const {
    
    if (word.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

    // Existence required
double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void SearchServer::Query::MakeUnique() {
            sort(plus_words.begin(), plus_words.end());
            sort(minus_words.begin(), minus_words.end());
            auto it_plus = unique(plus_words.begin(), plus_words.end());
            auto it_minus = unique(minus_words.begin(), minus_words.end());
            plus_words.resize(std::distance(plus_words.begin(), it_plus));
            minus_words.resize(std::distance(minus_words.begin(), it_minus));
        }
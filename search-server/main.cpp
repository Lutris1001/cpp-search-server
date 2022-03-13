// #include "search_server.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <iostream>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAX_DELTA_ERROR = 1e-6;

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
        } else {
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

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
            : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    }

    explicit SearchServer(const string& stop_words_text)
            : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    SearchServer() = default;

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        ThrowExeptionsForDocumentAdding(document_id, document);

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
                           DocumentData{
                                   ComputeAverageRating(ratings),
                                   status
                           });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        auto filter = [status](int, DocumentStatus stat, int){return stat == status;};
        auto result = FindTopDocuments(raw_query, filter);
        return result;
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        ThrowExeptionsForArgument(raw_query);

        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < MAX_DELTA_ERROR) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });

        vector<Document> filtered_docs;
        for (const auto& doc : matched_documents) {
            if (document_predicate(doc.id, documents_.at(doc.id).status, doc.rating)) {
                filtered_docs.push_back(doc);
            }
        }

        if (filtered_docs.size() > MAX_RESULT_DOCUMENT_COUNT) {
            filtered_docs.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return filtered_docs;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) const {
        return documents_ids_.at(index);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        ThrowExeptionsForArgument(raw_query);

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
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_ = {};
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> documents_ids_;

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

    template <typename StringContainer>
    set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
        set<string> non_empty_strings;
        for (const string& str : strings) {
            if (!str.empty()) {
                if (!IsValidWord(str)) {
                    throw invalid_argument("Invalid stop words"s);
                }
                non_empty_strings.insert(str);
            }
        }
        return non_empty_strings;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
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
        return {
                text,
                is_minus,
                IsStopWord(text)
        };
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
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    void ThrowExeptionsForDocumentAdding(const int& document_id, const string& document_text) const {
        if (document_id < 0) {
            throw invalid_argument("Document have negative id."s);
        } else if (documents_.count(document_id) != 0) {
            throw invalid_argument("Document with same id is already exist."s);
        } else {
            const vector<string> words = SplitIntoWordsNoStop(document_text);
            for (const string& word : words) {
                if (!IsValidWord(word)) {
                    throw invalid_argument("The document contains restricted symbols."s);
                }
            }
        }
    }

    void ThrowExeptionsForArgument(const string& raw_query) const {
        for (const string& word : SplitIntoWordsNoStop(raw_query)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("The query contains restricted symbols."s);
            } else if ((word[0] == '-') && (word[1] == '-')) {
                throw invalid_argument("The query contains more then one minuses."s);
            } else if (word[word.size()-1] == '-') {
                throw invalid_argument("The query contains minus without text after it."s);
            }
        }
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
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
            matched_documents.push_back({
                                                document_id,
                                                relevance,
                                                documents_.at(document_id).rating
                                        });
        }
        return matched_documents;
    }
};

// -------- Макросы для юнит-тестирования --------

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


template <typename Function>
void RunTestImpl(const Function& function, const string& func_name) {
    function();
    cerr << func_name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)


// -------- Начало модульных тестов поисковой системы ----------

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
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestMinusWordsNotInResponse() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Searching does not work at all"s);
    }

    {
        SearchServer server;
        server.SetStopWords("the in"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-cat in barn"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Searching with minus words does not work"s);
    }
}

void TestMatchingResponse() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.MatchDocument("cat city"s, doc_id);
        auto x = get<0>(found_docs);
        ASSERT_EQUAL_HINT(x.size(), 2, "The document matching is not working"s);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.MatchDocument("cat -city"s, doc_id);
        auto x = get<0>(found_docs);
        ASSERT_EQUAL_HINT(x.size(), 0, "The document matching is not working with minus words"s);
    }
}

void TestSortingResponse() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 43;
    const string content2 = "dog in the barn"s;
    const vector<int> ratings2 = {8, 9, 7};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        const auto found_docs = server.FindTopDocuments("in city"s);
        for (int i = 0; i < static_cast<int>(found_docs.size()) - 1; ++i) {
            ASSERT_HINT(found_docs.at(i).relevance >= found_docs.at(i+1).relevance, "The sorting by relevance is working incorrect"s);
        }
    }
}


void TestStatusInResponse() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 43;
    const string content2 = "dog in the barn"s;
    const vector<int> ratings2 = {8, 9, 7};
    const int doc_id3 = 44;
    const string content3 = "rat in the cage"s;
    const vector<int> ratings3 = {3, 4, 8};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        const auto found_docs = server.FindTopDocuments("in"s, DocumentStatus::BANNED);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "It was found an incorrect number of documents"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id2, "The found document have wrong status"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 2, "Preset document status is not working as excepted"s);
    }
}

void TestRatingResponse() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {4, 5, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        double rate = ((4 + 5 + 3) * 1.0) / 3.0;
        ASSERT(abs(doc0.rating - rate) <= MAX_DELTA_ERROR);
    }
}


void TestPredictResponse() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 43;
    const string content2 = "dog in the barn"s;
    const vector<int> ratings2 = {8, 9, 7};
    const int doc_id3 = 44;
    const string content3 = "rat in the cage"s;
    const vector<int> ratings3 = {3, 4, 8};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        const auto found_docs = server.FindTopDocuments("in"s, [](int id, DocumentStatus, int) {return id % 2 == 0;});
        for (int i = 0; i < static_cast<int>(found_docs.size()); ++i) {
            ASSERT_HINT(found_docs.at(i).id % 2 == 0, "The sorting function is not working. ID is not even."s);
        }
    }
}


void TestRelevanceResponse() {
    const int doc_id = 42;
    const string content = "белый кот и модный ошейник"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 43;
    const string content2 = "пушистый кот пушистый хвост"s;
    const vector<int> ratings2 = {8, 9, 7};
    const int doc_id3 = 44;
    const string content3 = "ухоженный пёс выразительные глаза"s;
    const vector<int> ratings3 = {3, 4, 8};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT(abs(found_docs.at(0).relevance - 0.6507) <= 0.03);
        ASSERT(abs(found_docs.at(1).relevance - 0.2746) <= 0.03);
        ASSERT(abs(found_docs.at(2).relevance - 0.1014) <= 0.03);
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWordsNotInResponse);
    RUN_TEST(TestSortingResponse);
    RUN_TEST(TestMatchingResponse);
    RUN_TEST(TestRatingResponse);
    RUN_TEST(TestStatusInResponse);
    RUN_TEST(TestPredictResponse);
    RUN_TEST(TestRelevanceResponse);
}

// --------- Окончание модульных тестов поисковой системы -----------

// ------------ Пример использования ----------------

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);

    return 0;
}
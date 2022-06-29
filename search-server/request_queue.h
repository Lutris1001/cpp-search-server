#pragma once
#include <vector>
#include <string>
#include <deque>
#include "document.h"
#include "search_server.h"

using namespace std;

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate);

    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status);

    vector<Document> AddFindRequest(const string& raw_query);

    int GetNoResultRequests() const;
    
private:
    
    struct QueryResult {
        vector<Document> result;
        bool empty_result;
    };
    
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int now_time = 0;
    const SearchServer& search_server_;
    int empty_requests_ = 0;
    
    void StepForNewRequest();
    
    void StepAfterRequest(const vector<Document>& result);
};

template <typename DocumentPredicate>
vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
    StepForNewRequest();
    vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
    StepAfterRequest(result);
    return result;
}
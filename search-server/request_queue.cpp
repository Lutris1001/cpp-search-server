#include <vector>
#include <deque>
#include "request_queue.h"
#include "document.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
    {
    }

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    StepForNewRequest();
    vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
    StepAfterRequest(result);
    return result;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    StepForNewRequest();
    vector<Document> result = search_server_.FindTopDocuments(raw_query);
    StepAfterRequest(result);
    return result;
}

int RequestQueue::GetNoResultRequests() const {
    return empty_requests_;
}

void RequestQueue::StepForNewRequest() {
    ++now_time;
    if (now_time > min_in_day_ || requests_.size() >= min_in_day_) {
        if ((requests_.front()).empty_result) {
            --empty_requests_;
        }
        requests_.pop_front();
    }
}
    
void RequestQueue::StepAfterRequest(const vector<Document>& result) {
    bool empty_result = !!(result.empty());
    if (empty_result) {
        ++empty_requests_;
    }
    QueryResult request = {result, empty_result};
    requests_.push_back(request);
}
    

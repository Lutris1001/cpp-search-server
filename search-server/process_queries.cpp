#include <vector>
#include <list>
#include <string>
#include <string_view>
#include <execution>
#include <algorithm>
#include <utility>

#include "document.h"
#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    
    std::vector<std::vector<Document>> documents_lists(queries.size());
    
    std::transform(std::execution::par,
        queries.begin(), queries.end(),
        documents_lists.begin(),
        [&search_server](const auto& query){ return search_server.FindTopDocuments(query); });
    
    return documents_lists;
}


std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    
    std::list<Document> documents_list;
    
    auto tmp = ProcessQueries(search_server, queries);
    
    for (auto& i : tmp) {
        for (auto& doc : i) {
            documents_list.push_back(move(doc));
        }
    }
    return documents_list;
} 


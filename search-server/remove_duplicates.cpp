// в качестве заготовки кода используйте последнюю версию своей поисковой системы
//#include <vector>
#include <iostream>
#include <set>
#include "search_server.h"
#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    set<int> ids_to_remove;
      
    for (const int document_id : search_server) {
        set<string> words;
        for (const auto& [key, value] : search_server.GetWordFrequencies(document_id)) {
            words.insert(key);
        }
        
        for (auto x = upper_bound(search_server.begin(), search_server.end(), document_id); x != search_server.end(); ++x) {
            set<string> else_words;
            for (const auto& [key2, value2] : search_server.GetWordFrequencies(*x)) {
                else_words.insert(key2);
            }
            if (words == else_words) {
                ids_to_remove.insert(*x);
            }
        }
    }
    
    for (int id : ids_to_remove) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}
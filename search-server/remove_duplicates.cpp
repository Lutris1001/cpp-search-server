#include <iostream>
#include <set>
#include "search_server.h"
#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    set<int> ids_to_remove;
    set<set<string>> original_docs; // Набор оригинальных документов

    for (const int document_id : search_server) {
        set<string> words;
        for (const auto& [key, value] : search_server.GetWordFrequencies(document_id)) {
            words.insert(key);
        }
        // Если документ является дубликатом - то заносим его в список на удаление
        // при этом количество проверок внутри find растет только при увеличении количества
        // оригинальных документов и игнорируя дубликаты
        if (original_docs.find(words) != original_docs.end()) {
            ids_to_remove.insert(document_id);
        } else {
            original_docs.insert(words); // иначе записываем как оригинальный набор слов
        }
    }

    for (int id : ids_to_remove) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}

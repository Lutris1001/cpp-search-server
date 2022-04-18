#include <iostream>
#include <set>
#include "search_server.h"
#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    set<int> ids_to_remove;
    static set<set<string>> original_docs; // Набор оригинальных документов
    static auto last_id = search_server.begin(); // При повторном вызове функции
    // если я все правильно сделал, то проверяться будут только новые документы

    for (auto id = last_id; id != search_server.end(); ++id) {
        set<string> words;
        for (const auto& [key, value] : search_server.GetWordFrequencies(*id)) {
            words.insert(key);
        }
        // Если документ является дубликатом - то заносим его в список на удаление
        // при этом количество проверок внутри find растет только при увеличении количества
        // оригинальных документов и игнорируя дубликаты
        if (original_docs.find(words) != original_docs.end()) {
            ids_to_remove.insert(*id);
        } else {
            original_docs.insert(words); // иначе записываем как оригинальный набор слов
            last_id = id;
        }
    }

    for (int id : ids_to_remove) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}

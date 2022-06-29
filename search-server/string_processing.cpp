#include <vector>
#include <string>
#include "string_processing.h"

using namespace std;

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.emplace_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.emplace_back(word);
    }

    return words;
}

vector<string> SplitIntoWordsView(string_view str) {
    vector<string> result;
    
    str.remove_prefix(min(str.find_first_not_of(" "), str.size()));
    
    int64_t pos = 0;
    
    const int64_t pos_end = str.npos;
    
    while (!(str.empty())) {
        int64_t space = str.find(" ");
        result.push_back(space == pos_end ? string(str.substr(pos)) : string(str.substr(pos, space)));
        str.remove_prefix(min(str.find_first_not_of(" ", space), str.size()));
    }

    return result;
}

vector<string> SplitIntoWords(const char* str) {
    vector<string> result;
    string_view tmp = std::string_view(str);
    
    tmp.remove_prefix(min(tmp.find_first_not_of(" "), tmp.size()));
    
    int64_t pos = 0;
    
    const int64_t pos_end = tmp.npos;
    
    while (!(tmp.empty())) {
        int64_t space = tmp.find(" ");
        result.push_back(space == pos_end ? string(tmp.substr(pos)) : string(tmp.substr(pos, space)));
        tmp.remove_prefix(min(tmp.find_first_not_of(" ", space), tmp.size()));
    }

    return result;
}

vector<string_view> SplitIntoWordsStringView(string_view str) {
    
    vector<string_view> result;
    
    str.remove_prefix(min(str.find_first_not_of(" "), str.size()));
    
    int64_t pos = 0;
    
    const int64_t pos_end = str.npos;
    
    while (!(str.empty())) {
        int64_t space = str.find(" ");
        result.push_back(space == pos_end ? (str.substr(pos)) : (str.substr(pos, space)));
        str.remove_prefix(min(str.find_first_not_of(" ", space), str.size()));
    }
    
    return result;
}


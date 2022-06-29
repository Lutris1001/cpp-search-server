#pragma once
#include <string>
#include <vector>
#include <set>

using namespace std;

vector<string> SplitIntoWords(const string& text);

vector<string> SplitIntoWordsView(string_view str);

vector<string> SplitIntoWords(const char* str);

vector<string_view> SplitIntoWordsStringView(string_view str);

template <typename StringContainer>
set<string, less<>> MakeUniqueNonEmptyStrings(const StringContainer& container) {
    set<string, less<>> non_empty_strings;
    for (auto i = make_move_iterator(container.begin()); i != make_move_iterator(container.end()); ++i) {
        if (!(*i).empty()) {
            non_empty_strings.insert(static_cast<string>(*i));
        }
    }
    return non_empty_strings;
}
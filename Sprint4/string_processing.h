#pragma once
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

std::string ReadLine();
int ReadLineWithNumber();
std::vector<std::string_view> SplitIntoWords(std::string_view sv);

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;

    for (auto& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(std::string(str));
        }
    }

    return non_empty_strings;
}

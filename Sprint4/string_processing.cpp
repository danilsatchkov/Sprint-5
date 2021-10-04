#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(std::string_view sv) {
    std::vector<std::string_view> words;
    size_t pos = 0;
    while (true) {
        auto space = sv.find(' ', pos);
        words.push_back(sv.substr(0, space));

        if (space == sv.npos) {
            break;
        }
        else {
            sv.remove_prefix(space + 1);
        }
    }
    
    return words;
}
#include "remove_duplicates.h"     

void RemoveDuplicates(SearchServer& search_server) {

    // need to loop through the search server, find word set for every document
    //compare set to map, if document_id is higher than the one in word_to_docs_ then remove it from search server

    std::set<int>::const_iterator it = search_server.begin();
    std::map<std::set<std::string>, int> words_to_doc;
    std::vector<int> docs_to_delete;


    while (it != search_server.end())
    {
        std::set<std::string> word_set;
        for (auto words_freqs : search_server.GetWordFrequencies(*it)) {
            word_set.insert(words_freqs.first);
        }
        if (words_to_doc.find(word_set) == words_to_doc.end()) {
            words_to_doc[word_set] = *it;
        }
        else if (*it > words_to_doc[word_set])
        {
            words_to_doc[word_set] = *it;
            docs_to_delete.push_back(*it);
        }
        ++it;
    }
    for (auto d : docs_to_delete) {
        std::cout << "Found duplicate document id " << d << std::endl;
        search_server.RemoveDocument(d);
    }
}
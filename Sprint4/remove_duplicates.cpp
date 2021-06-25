#include "remove_duplicates.h"     

void RemoveDuplicates(SearchServer& search_server) {

// need to loop through the search server, find word set for every document
//compare set to map, if document_id is higher than the one in word_to_docs_ then remove it from search server

    std::vector<int>::const_iterator it = search_server.begin();
    while (it != search_server.end())
    {
        std::set<std::string> word_set;
        for (auto words_freqs : search_server.GetWordFrequencies(*it)) {
            word_set.insert(words_freqs.first);
        }

        if (*it > search_server.words_to_doc_[word_set])
        {
            std::cout << "Found duplicate document id " << *it << std::endl;
            search_server.RemoveDocument(*it);
        }
        else  ++it;
    }
    }

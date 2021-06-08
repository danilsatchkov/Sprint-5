#pragma once
#include <deque>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include "document.h"
#include "search_server.h"



class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) ;
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) ;

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        QueryResult(bool empty) :empty_(empty) {}
        bool empty_;

    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& search_server_;
    int num_empty_ = 0;
};


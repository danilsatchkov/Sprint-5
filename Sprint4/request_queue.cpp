#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_(search_server){
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    std::vector <Document> result = search_server_.FindTopDocuments(raw_query, status);
    if (requests_.size() == 1440)
    {
        if (requests_.back().empty_ == true) --num_empty_;
        requests_.pop_back();
    }
    if (result.empty())
    {
        requests_.push_front(true);
        ++num_empty_;
    }
    else requests_.push_front(false);

    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query);
    if (requests_.size() == 1440)
    {
        if (requests_.back().empty_ == true) --num_empty_;
        requests_.pop_back();
    }
    if (result.empty())
    {
        requests_.push_front(true);
        ++num_empty_;
    }
    else requests_.push_front(false);

    return result;
}

int RequestQueue::GetNoResultRequests() const {
    return num_empty_;
}

 
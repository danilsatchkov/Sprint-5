#pragma once
#include <map>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    int num_maps;
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
    private:
        std::lock_guard<std::mutex> guard;
    public:
        Value& ref_to_value;

        Access(const Key& key, std::map<Key, Value>& map, std::mutex& mut)
            : guard(mut), ref_to_value(map[key])  {}
    };

        explicit ConcurrentMap(size_t bucket_count) :num_maps(bucket_count), buckets_(bucket_count) {
        }

        Access operator[](const Key& key) {
            int mapnum = static_cast<int>(static_cast<uint64_t>(key) % num_maps);

            return  Access(key, buckets_[mapnum].map, buckets_[mapnum].mutex);
        }

        void Erase(const Key& key) {
            int mapnum = static_cast<int>(static_cast<uint64_t>(key) % num_maps);
            std::lock_guard<std::mutex> guard(buckets_[mapnum].mutex);
            buckets_[mapnum].map.erase(key);
        }

        std::map<Key, Value> BuildOrdinaryMap() {
            std::map<Key, Value> res;

            for (auto& bucket : buckets_) {
                std::lock_guard<std::mutex> guard(bucket.mutex);
                res.merge(bucket.map);
            }
            return res;
        }

    private:
        struct Bucket {
            std::mutex mutex;
            std::map<Key, Value> map;
        };
        std::vector<Bucket> buckets_;
    };

template <typename T,typename Compare = std::less<T>> 
class ConcurrentSet {
public:
    const int num_sets_=16;    
    explicit ConcurrentSet(size_t bucket_count) :num_sets_(bucket_count), buckets_(bucket_count) {
    }

    void Insert(const T& value) {
        int cur_set = rand() % num_sets_;
        std::lock_guard<std::mutex> guard(buckets_[cur_set].mutex);
        buckets_[cur_set].set.insert(value);        
    }

    std::set<T,Compare> BuildOrdinarySet() {
        std::set<T,Compare> res;

        for (auto& bucket : buckets_) {
            std::lock_guard<std::mutex> guard(bucket.mutex);
            if (!bucket.set.empty()) {
                res.insert(bucket.set.begin(), bucket.set.end());               
            }
        }
        return res;
    }

private:    
    struct Bucket {
        std::mutex mutex;
        std::set<T,Compare> set;
    };
    std::vector<Bucket> buckets_;

};

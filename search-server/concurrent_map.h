#include <mutex>
#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <string>
#include <atomic>
#include <execution>

#include "log_duration.h"
#include "test_framework.h"

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        
        Access(std::mutex& mut, Value& ref) 
            : guard(mut), ref_to_value(ref)
        {
        }

        ~Access() {
            guard.unlock();
        }
        
        std::mutex& guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) 
        : BUCKET_COUNT(bucket_count), part_mutex_(bucket_count), divided_map_(bucket_count)
    {
    }

    Access operator[](const Key& key) {
    
        for (int i = 0; i < static_cast<int>(divided_map_.size()); ++i) {
            
            if (divided_map_[i].count(key) != 0) {
                part_mutex_[i].lock();
                return {part_mutex_[i], divided_map_[i].at(key)};
            }
        }
        uint64_t mutex_num = next_insertion_.fetch_add(1) % BUCKET_COUNT;
        part_mutex_[mutex_num].lock();
        return {part_mutex_[mutex_num], divided_map_[mutex_num][key]};
    }
    
    void erase(const Key& key) {
        for (int i = 0; i < static_cast<int>(divided_map_.size()); ++i) {
            if (divided_map_[i].count(key) != 0) {
                std::lock_guard guard(part_mutex_[i]);
                divided_map_[i].erase(key);
            }
        }
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> ordinary_map;
        for (int i = 0; i < static_cast<int>(divided_map_.size()); ++i) {
            std::lock_guard guard(part_mutex_[i]);
            ordinary_map.merge(divided_map_[i]);
        }
        return ordinary_map;
    }

private:
    const uint64_t BUCKET_COUNT;
    std::vector<std::mutex> part_mutex_;
    std::vector<std::map<Key, Value>> divided_map_;
    std::atomic_uint64_t next_insertion_ = 0;
};
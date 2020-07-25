#include <deque>

#define DEBUG_MODE
#define DEFAULT_BTREE_FACTOR 6

#include <btree.hpp>

#define LIMIT 20000
#define POP_LIMIT 10000
using namespace btree;

int main() {
    auto seed = time(nullptr);
    std::cout << seed << std::endl;
    srand(seed);
    {
        std::vector<int> a, b;
        BTree<int, int> test;
        for (int i = 0; i < LIMIT; ++i) {
            auto k = rand();
            a.push_back(k);
            test.insert(k, k);
        }
        std::sort(a.begin(), a.end());
        a.erase(unique(a.begin(), a.end()), a.end());
        for (auto i = 0; i < POP_LIMIT; ++i) {
            auto step = rand() % a.size();
            auto iter0 = a.begin();
            auto iter1 = test.begin();
            std::advance(iter0, step);
            for (int i = 0; i < step; ++i, ++iter1);
            a.erase(iter0);
            test.erase(iter1);
        }
        for (auto i : test) {
            b.push_back(i.first);
        }
        assert(a == b);
    }
    {
        std::deque<int> a, b;
        BTree<int, int> test;
        for (int i = 0; i < LIMIT; ++i) {
            auto k = rand();
            a.push_back(k);
            test.insert(k, k);
        }
        std::sort(a.begin(), a.end());
        a.erase(unique(a.begin(), a.end()), a.end());
        for (auto i = 0; i < POP_LIMIT; ++i) {
            a.pop_front();
            test.pop_min();
        }
        for (auto i : test) {
            b.push_back(i.first);
        }
        assert(a == b);
    }

    {
        std::vector<int> a, b;
        BTree<int, int> test;
        for (int i = 0; i < LIMIT; ++i) {
            auto k = rand();
            a.push_back(k);
            test.insert(k, k);
        }
        std::sort(a.begin(), a.end());
        a.erase(unique(a.begin(), a.end()), a.end());
        for (auto i = 0; i < POP_LIMIT; ++i) {
            a.pop_back();
            test.pop_max();
        }
        for (auto i : test) {
            b.push_back(i.first);
        }
        assert(a == b);
    }
    return 0;
}
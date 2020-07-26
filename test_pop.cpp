#include <deque>

#define DEBUG_MODE
#define DEFAULT_BTREE_FACTOR 3

#include <btree.hpp>

#define LIMIT 20000
#define POP_LIMIT 20000
#ifndef __OPTIMIZE__
#define TEST(x) assert((x))
#else
#define TEST(x) if (!(x)) std::abort();
#endif
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
        TEST(test.size() == a.size());
        for (auto i = 0; i < POP_LIMIT; ++i) {
            auto step = rand() % a.size();
            auto iter0 = a.begin();
            auto iter1 = test.begin();
            std::advance(iter0, step);
            for (unsigned i = 0; i < step; ++i, ++iter1);
            a.erase(iter0);
            test.erase(iter1);
            TEST(a.size() == test.size());
        }
        for (auto i : test) {
            b.push_back(i.first);
        }
        TEST(a == b);
    }
    TEST(alive_node == 0);
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
        while (test.size()) {
            a.pop_front();
            test.pop_min();
            TEST(a.size() == test.size());
        }
        for (auto i : test) {
            b.push_back(i.first);
        }
        TEST(a == b);
    }
    TEST(alive_node == 0);
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
        while (test.size()) {
            a.pop_back();
            test.pop_max();
            TEST(a.size() == test.size());
        }
        for (auto i : test) {
            b.push_back(i.first);
        }
        TEST(a == b);
    }
    TEST(alive_node == 0);
    return 0;
}
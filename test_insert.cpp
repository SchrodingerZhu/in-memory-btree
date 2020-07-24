#include <vector>
#include <random>
#define DEBUG_MODE
#include <btree.hpp>
#define LIMIT 100000

int main() {
    //auto seed = time(nullptr);
    //std::cout << seed << std::endl;
    //srand(seed);
    std::vector<int> a, b;
    BTree<int, int> test;
    for (int i = 0; i < LIMIT; ++i) {
        auto k = rand();
        a.push_back(k);
        test.insert(k, k);
    }
    std::sort(a.begin(), a.end());
    a.erase(unique(a.begin(), a.end()), a.end());
    for (auto i : test) {
        b.push_back(i.first);
    }
    assert(a == b);
    for (int i = 0; i <  LIMIT; ++i) {
        auto target = rand();
        auto A = std::binary_search(a.begin(), a.end(), target);
        auto B = test.member(target);
        assert(A == B);
    }
    std::random_shuffle(a.begin(), a.end());
    for (auto i : a) {
        assert(test.member(i));
    }
    return 0;
}
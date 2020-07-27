#include <vector>
#include <random>

#define DEBUG_MODE
#define DEFAULT_BTREE_FACTOR 6

#include <btree.hpp>

#define LIMIT 20

using namespace btree;

int main() {
    {
        auto seed = time(nullptr);
        std::cout << seed << std::endl;
        srand(seed);
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
        ASSERT(a == b);
        {
            auto copied = test;
            std::vector<int> c;
            for (auto i : copied) {
                c.push_back(i.first);
            }
            ASSERT(a == b);
            ASSERT(test.size() == copied.size());
        }
        for (int i = 0; i < LIMIT; ++i) {
            auto target = rand();
            auto A = std::binary_search(a.begin(), a.end(), target);
            auto B = test.member(target);
            ASSERT(A == B);
        }
        std::random_shuffle(a.begin(), a.end());
        for (auto i : a) {
            ASSERT(test.member(i));
        }
    }
    ASSERT(alive_node == 0);
    return 0;
}
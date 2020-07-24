#include <map>

#define DEFAULT_BTREE_FACTOR 6
#define BINARY_SEARCH
#include <iostream>
#include <btree.hpp>
#include <chrono>
#include <random>
#define LIMIT 200000
#define SEED 0x114514
template<typename F, typename... Args>
void timeit(F f, Args&&... args) {
    auto start = std::chrono::high_resolution_clock::now();
    f(std::forward<Args>(args)...);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "microsecs: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
}

struct Rng {
    std::mt19937_64 engine;
    std::uniform_int_distribution<int> dist;
    Rng() : engine(SEED) {}
    int operator()() {
        return dist(engine);
    }
};
int main() {
    {
        auto limit = 10'000'000;
        std::cout << limit << " insertions (map)" << std::endl;
        auto rng = Rng();
        timeit([&]{
            std::map<int, int> tester;
            for (int i = 0; i < limit; ++i) {
                tester.insert({rng(), rng()});
            }
        });
    }
    {
        auto limit = 10'000'000;
        std::cout << limit << " insertions (btree)" << std::endl;
        auto rng = Rng();
        timeit([&]{
            BTree<int, int> tester;
            for (int i = 0; i < limit; ++i) {
                tester.insert(rng(), rng());
            }
        });
    }

    {
        auto limit = 10'000'000;
        std::cout << limit << " membership (map)" << std::endl;
        auto rng = Rng();
        std::map<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert({rng(), rng()});
        }
        timeit([&]{
            for (int i = 0; i < limit; ++i) {
                tester.count(rng());
            }
        });
    }

    {
        auto limit = 10'000'000;
        std::cout << limit << " membership (btree)" << std::endl;
        auto rng = Rng();
        BTree<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert(rng(), rng());
        }
        timeit([&]{
            for (int i = 0; i < limit; ++i) {
                tester.member(rng());
            }
        });
    }

    {
        auto limit = 10'000'000;
        std::cout << limit << " erase min (map)" << std::endl;
        auto rng = Rng();
        std::map<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert({rng(), rng()});
        }
        timeit([&]{
            while (!tester.empty()) {
                tester.erase(tester.begin());
            }
        });
    }

    {
        auto limit = 10'000'000;
        std::cout << limit << " erase min (btree)" << std::endl;
        auto rng = Rng();
        BTree<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert(rng(), rng());
        }
        timeit([&]{
            while (!tester.empty()) {
                tester.erase(tester.begin());
            }
        });
    }
    return 0;
}
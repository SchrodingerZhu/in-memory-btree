#include <map>

#define DEFAULT_BTREE_FACTOR 6

#include <iostream>
#include <btree.hpp>
#include <chrono>
#include <random>

#define LIMIT 200000
#define SEED 0x114514
using namespace btree;
template<typename F, typename... Args>
void timeit(F f, Args &&... args) {
    auto start = std::chrono::high_resolution_clock::now();
    f(std::forward<Args>(args)...);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "microsecs: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
              << std::endl;
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
    auto rng = Rng();
    std::vector<int> data(10'000'000);
    std::vector<int> codata(10'000'000);
    for(auto& i : data) {
        i = rng();
    }
    for(auto& i : codata) {
        i = rng();
    }
    {
        auto limit = 10'000'000;
        std::cout << limit << " insertions (map)" << std::endl;
        timeit([&] {
            std::map<int, int> tester;
            for (int i = 0; i < limit; ++i) {
                tester.insert({data[i], data[i]});
            }
        });
    }
    {
        auto limit = 10'000'000;
        std::cout << limit << " insertions (btree)" << std::endl;
        timeit([&] {
            BTree<int, int> tester;
            for (int i = 0; i < limit; ++i) {
                tester.insert(data[i], data[i]);
            }
        });
    }

    auto M = 0;
    {
        auto limit = 10'000'000;
        std::cout << limit << " membership (map)" << std::endl;
        std::map<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert({data[i], data[i]});
        }
        timeit([&] {
            for (int i = 0; i < limit; ++i) {
                M += tester.count(codata[i]);
            }
        });
    }

    auto N = 0;
    {
        auto limit = 10'000'000;
        std::cout << limit << " membership (btree)" << std::endl;
        BTree<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert(data[i], data[i]);
        }
        timeit([&] {
            for (int i = 0; i < limit; ++i) {
                N += tester.member(codata[i]);
            }
        });
    }
    if (M != N) std::abort();
    {
        auto limit = 10'000'000;
        std::cout << limit << " erase min (map)" << std::endl;
        std::map<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert({data[i], data[i]});
        }
        timeit([&] {
            while (!tester.empty()) {
                tester.erase(tester.begin());
            }
        });
    }

    {
        auto limit = 10'000'000;
        std::cout << limit << " erase min (btree)" << std::endl;
        BTree<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert(data[i], data[i]);
        }
        timeit([&] {
            while (!tester.empty()) {
                tester.erase(tester.begin());
            }
        });
    }
    size_t A = 0;
    {
        auto limit = 10'000'000;
        std::cout << limit << " iterate through (map)" << std::endl;
        std::map<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert({data[i], data[i]});
        }
        timeit([&] {
            auto iter = tester.begin();
            while (iter != tester.end()) {
                A ^= iter->first;
                ++iter;
            }
        });
    }
    size_t B = 0;
    {
        auto limit = 10'000'000;
        std::cout << limit << " iterate through (btree)" << std::endl;
        BTree<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert(data[i], data[i]);
        }
        timeit([&] {
            auto iter = tester.begin();
            while (iter != tester.end()) {
                B ^= (*iter).first;
                ++iter;
            }
        });
    }
    if (A != B) std::abort();

    {
        auto limit = 10'000'000;
        std::cout << limit << " copy construct (map)" << std::endl;
        std::map<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert({data[i], data[i]});
        }
        timeit([&] {
            auto another = tester;
        });
    }
    {
        auto limit = 10'000'000;
        std::cout << limit << " copy construct (btree)" << std::endl;
        BTree<int, int> tester;
        for (int i = 0; i < limit; ++i) {
            tester.insert(data[i], data[i]);
        }
        timeit([&] {
            auto another = tester;
        });
    }


}
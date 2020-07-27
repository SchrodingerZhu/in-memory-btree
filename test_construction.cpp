#include <vector>
#include <random>

#define DEBUG_MODE

#define DEFAULT_BTREE_FACTOR 6

#include <btree.hpp>
#include <set>
#define LIMIT 20000
#define POP_LIMIT 20000

using namespace btree;

struct Cell {
    static size_t ctor, dtor, alive;
    char *tag;

    Cell() {
        ctor++;
        alive++;
        tag = new char;
    }

    Cell(const Cell &) {
        ctor++;
        alive++;
        tag = new char;
    }

    Cell(Cell &&that) {
        ctor++;
        tag = that.tag;
        that.tag = nullptr;
    }

    Cell &operator=(Cell &&that) {
        delete (tag);
        alive -= (tag != nullptr);
        tag = that.tag;
        that.tag = nullptr;
        return *this;
    };

    ~Cell() {
        dtor++;
        alive -= (tag != nullptr);
        delete (tag);
    }
};

size_t Cell::ctor = 0;
size_t Cell::dtor = 0;
size_t Cell::alive = 0;

int main(int argc, char** argv) {
    auto seed = argc > 1 ? std::atoi(argv[1]) : time(nullptr);
    std::cout << seed << std::endl;
    srand(seed);
    {
        BTree<int, Cell> test;
        std::set<int> u;
        for (int i = 0; i < LIMIT; ++i) {
            auto k = rand();
            test.insert(k, Cell());
            u.insert(k);
            ASSERT(Cell::alive == u.size());
        }

    }
    std::cout << "ctor: " << Cell::ctor << ", dtor: " << Cell::dtor << std::endl;
    ASSERT(Cell::ctor == Cell::dtor);
    ASSERT(alive_node == 0);
    ASSERT(Cell::alive == 0);
    {
        BTree<int, Cell> test;
        for (int i = 0; i < LIMIT; ++i) {
            test.insert(rand(), Cell());
        }
        while (!test.empty()) {
            if (rand() & 1) {
                test.pop_max();
            } else {
                test.pop_min();
            }
        }
    }
    std::cout << "ctor: " << Cell::ctor << ", dtor: " << Cell::dtor << std::endl;
    ASSERT(Cell::ctor == Cell::dtor);
    ASSERT(alive_node == 0);
    ASSERT(Cell::alive == 0);
}
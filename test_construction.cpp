#include <vector>
#include <random>

#define DEBUG_MODE
#define BINARY_SEARCH
#define DEFAULT_BTREE_FACTOR 6
#include <btree.hpp>

#define LIMIT 8000

struct Cell {
    static size_t ctor, dtor;
    char *tag;

    Cell() {
        ctor++;
        tag = new char;
    }

    Cell(const Cell &) {
        ctor++;
        tag = new char;
    }

    Cell(Cell &&that) {
        ctor++;
        tag = that.tag;
        that.tag = nullptr;
    }

    Cell &operator=(Cell &&that) {
        delete (tag);
        tag = that.tag;
        that.tag = nullptr;
        return *this;
    };

    ~Cell() {
        dtor++;
        delete (tag);
    }
};

size_t Cell::ctor = 0;
size_t Cell::dtor = 0;

int main() {
    //auto seed = time(nullptr);
    //std::cout << seed << std::endl;
    srand(0);
    {
        BTree<int, Cell> test;
        for (int i = 0; i < LIMIT; ++i) {
            test.insert(rand(), Cell());
        }
    }
    std::cout << "ctor: " << Cell::ctor << ", dtor: " << Cell::dtor << std::endl;
    assert(Cell::ctor == Cell::dtor);
}
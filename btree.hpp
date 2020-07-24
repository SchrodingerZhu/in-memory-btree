#ifndef BTREE_HPP
#define BTREE_HPP

#include <algorithm>
#include <cstring>

#define FOUND (1u << 16u)
#define FOUND_MASK (FOUND - 1u)
#define GO_DOWN (1u << 24u)
#define GO_DOWN_MASK (GO_DOWN - 1u)
#ifndef DEFAULT_BTREE_FACTOR
#define DEFAULT_BTREE_FACTOR 6
#endif
#ifdef DEBUG_MODE

#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>

#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif

template<typename K, typename V, size_t B = DEFAULT_BTREE_FACTOR, typename Compare = std::less<K>>
class BTree;

template<typename K, typename V, size_t B = DEFAULT_BTREE_FACTOR, typename Compare = std::less<K>>
class AbstractBTNode;

template<typename K, typename V, bool IsInternal, typename Compare = std::less<K>, size_t B = DEFAULT_BTREE_FACTOR>
struct alignas(64) BTreeNode;

template<typename K, typename V, size_t B, typename Compare>
struct AbstractBTNode {

    static Compare comp;

    struct SplitResult {
        AbstractBTNode *l, *r;
        K key;
        V value;
    };

    struct iterator {
        uint16_t idx;
        AbstractBTNode *node;

        bool operator==(const iterator &that) noexcept {
            return idx == that.idx && node == that.node;
        }

        bool operator!=(const iterator &that) noexcept {
            return idx != that.idx || node != that.node;
        }

        iterator operator++(int) {
            return node->successor(idx);
        }

        iterator &operator++() {
            auto res = node->successor(idx);
            this->node = res.node;
            this->idx = res.idx;
            return *this;
        }

        std::pair<const K &, V &> operator*() {
            return {node->key_at(idx), node->value_at(idx)};
        }
    };

    virtual bool member(const K &key) = 0;

    virtual std::optional<V> insert(const K &key, const V &value, AbstractBTNode **root) = 0;

    virtual void
    adopt(AbstractBTNode *l, AbstractBTNode *r, K key, V value, size_t position, AbstractBTNode **root) = 0;

    virtual void fix_underflow(AbstractBTNode **root) = 0;

    virtual AbstractBTNode *&node_parent() = 0;

    virtual uint16_t &node_idx() = 0;

    virtual uint16_t &node_usage() = 0;

    virtual iterator successor(uint16_t idx) = 0;

    virtual iterator predecessor(uint16_t idx) = 0;

    virtual iterator min() = 0;

    virtual iterator max() = 0;

    virtual K &key_at(size_t) = 0;

    virtual V &value_at(size_t) = 0;

    virtual std::pair<K, V> erase(uint16_t index, AbstractBTNode **root) = 0;

    virtual AbstractBTNode *&child_at(size_t) = 0;

    virtual ~AbstractBTNode() = default;

#ifdef DEBUG_MODE

    virtual void display(size_t indent) = 0;

#endif

    friend BTree<K, V, B, Compare>;
};

template<typename K, typename V, size_t B, typename Compare>
class BTree {
    AbstractBTNode<K, V, B, Compare> *root = nullptr;
public:
    using iterator = typename AbstractBTNode<K, V, B, Compare>::iterator;
#ifdef DEBUG_MODE

    void display() {
        if (root) root->display(0);
    };
#endif

    std::optional<V> insert(const K &key, const V &value) {
        if (root == nullptr) {
            auto node = new BTreeNode<K, V, false, Compare, B>;
            node->usage = 1;
            node->keys[0] = key;
            node->values[0] = value;
            root = node;
            return std::nullopt;
        }
        return root->insert(key, value, &root);
    }

    bool empty() {
        return !root || root->node_usage() == 0;
    }

    bool member(const K& key) {
        return root->member(key);
    }

    const K &min_key() {
        auto iter = root->min();
        return iter.node->key_at(iter.idx);
    }

    const K &max_key() {
        auto iter = root->max();
        return iter.node->key_at(iter.idx);
    }

    iterator begin() {
        if (root)
            return root->min();
        return end();
    }

    iterator end() {
        return iterator{
                .idx = 0,
                .node = nullptr
        };
    }

    ~BTree() {
        delete root;
    }

    std::pair<K, V> erase(iterator iter) {
        return iter.node->erase(iter.idx, &root);
    }

    std::pair<K, V> pop_min() {
        auto iter = root->min();
        return erase(iter);
    }

    std::pair<K, V> pop_max() {
        auto iter = root->max();
        return erase(iter);
    }
};

template<typename K, typename V, bool IsInternal, typename Compare, size_t B>
struct alignas(64) BTreeNode : AbstractBTNode<K, V, B, Compare> {
    static_assert(2 * B < FOUND, "B is too large");
    static_assert(B > 2, "B is too small");
    using Node = AbstractBTNode<K, V, B, Compare>;
    using NodePtr = Node *;
    using SplitResult = typename AbstractBTNode<K, V, B, Compare>::SplitResult;

    K keys[2 * B - 1];
    V values[2 * B - 1];
    NodePtr children[IsInternal ? (2 * B) : 0];
    NodePtr parent = nullptr;
    uint16_t usage = 0;
    uint16_t parent_idx = 0;

    using LocFlag = uint;


    inline NodePtr &node_parent() override {
        return parent;
    }

    inline uint16_t &node_usage() override {
        return usage;
    }

    inline uint16_t &node_idx() override {
        return parent_idx;
    }

    inline LocFlag local_search(const K &key) {
        ASSERT(usage < 2 * B);
#ifdef BINARY_SEARCH
        uint16_t position = std::lower_bound(keys, keys + usage, key, Node::comp) - keys;
        if (position != usage && !Node::comp(key, keys[position])) {
            return FOUND | position;
        }
        return GO_DOWN | position;
#else
        uint i = 0;
        for (; i < usage && Node::comp(keys[i], key); ++i);
        if (i == usage) return GO_DOWN | usage;
        if (Node::comp(key, keys[i])) {
            return GO_DOWN | i;
        }
        return FOUND | i;
#endif
    }

    bool member(const K &key) override {
        ASSERT(usage < 2 * B);
        auto flag = local_search(key);
        if (flag & FOUND) {
            return true;
        }
        if constexpr (IsInternal) {
            return children[flag & GO_DOWN_MASK]->member(key);
        } else return false;
    }

    typename Node::SplitResult split() {
        ASSERT(usage == 2 * B - 1);
        auto l = new BTreeNode;
        auto r = new BTreeNode;
        l->usage = r->usage = B - 1;
        l->parent = r->parent = this->parent;
        std::uninitialized_move(keys, keys + B - 1, l->keys);
        std::uninitialized_move(keys + B, keys + usage, r->keys);
        std::uninitialized_move(values, values + B - 1, l->values);
        std::uninitialized_move(values + B, values + usage, r->values);
        this->usage = 0;
        if constexpr (IsInternal) {
            std::memcpy(l->children, children, B * sizeof(NodePtr));
            std::memcpy(r->children, children + B, B * sizeof(NodePtr));
            for (size_t i = 0; i < B; ++i) {
                l->children[i]->node_parent() = l;
                l->children[i]->node_idx() = i;
                r->children[i]->node_parent() = r;
                r->children[i]->node_idx() = i;
            }
        }
        return SplitResult{
                .l = l,
                .r = r,
                .key = std::move(keys[B - 1]),
                .value = std::move(values[B - 1]),
        };
    }

    NodePtr singleton(NodePtr l, NodePtr r, K key, V value) {
        auto node = new BTreeNode<K, V, true, Compare, B>;
        node->usage = 1;
        new(node->values) V(std::move(value));  // no need for destroy, directly move
        new(node->keys) K(std::move(key));
        node->children[0] = l;
        l->node_idx() = 0;
        l->node_parent() = node;
        node->children[1] = r;
        r->node_idx() = 1;
        r->node_parent() = node;
        return node;
    }

    std::optional<V> insert(const K &key, const V &value, NodePtr *root) override {
        auto res = local_search(key);
        if (res & FOUND) {
            V original = std::move(values[res & FOUND_MASK]);
            values[res & FOUND_MASK] = value;
            return {original};
        }
        auto position = res & GO_DOWN_MASK;
        if constexpr (IsInternal) {
            return children[position]->insert(key, value, root);
        } else {
            std::move_backward(values + position, values + usage, values + usage + 1);
            std::move_backward(keys + position, keys + usage, keys + usage + 1);
            values[position] = value;
            keys[position] = key;
            usage++;
            if (usage == 2 * B - 1) /* leaf if full */ {
                auto result = split();
                if (parent)
                    parent->adopt(result.l, result.r, std::move(result.key), std::move(result.value), parent_idx, root);
                else {
                    delete *root;
                    *root = singleton(result.l, result.r, std::move(result.key), std::move(result.value));
                }
            }
            return std::nullopt;
        }
    }

    void adopt(NodePtr l, NodePtr r, K key, V value, size_t position, NodePtr *root) override {
        std::move_backward(values + position, values + usage, values + usage + 1);
        std::move_backward(keys + position, keys + usage, keys + usage + 1);
        std::memmove(children + position + 1, children + position,
                     (usage + 1 - position) * sizeof(NodePtr));

        delete (children[position]);
        children[position] = l;
        l->node_parent() = this;
        children[position + 1] = r;
        r->node_parent() = this;
        for (int i = position; i < usage + 2; ++i) {
            children[i]->node_idx() = i;
        }
        values[position] = std::move(value);
        keys[position] = std::move(key);
        usage++;
        if (usage == 2 * B - 1) {
            auto result = split();
            if (parent) {
                parent->adopt(result.l, result.r, std::move(result.key), std::move(result.value), parent_idx, root);
            } else {
                delete *root;
                *root = singleton(result.l, result.r, std::move(result.key), std::move(result.value));
            }
        }
    }

    void borrow_left(NodePtr from) {
        ASSERT(dynamic_cast<BTreeNode *>(from));
        ASSERT(parent); // root will never borrow
        ASSERT(parent == dynamic_cast<BTreeNode *>(from)->parent);
        ASSERT(parent_idx > 0);
        ASSERT(parent_idx == from->node_idx() + 1);
        ASSERT(from->node_usage() - 1 >= B - 1);
        ASSERT(usage + 1 >= B - 1);

        std::move_backward(keys, keys + usage, keys + usage + 1);
        std::move_backward(values, values + usage, values + usage + 1);
        if constexpr (IsInternal) {
            std::memmove(children + 1, children, (usage + 1) * sizeof(NodePtr));
            for(auto i = 1; i <= usage + 1; ++i) {
                children[i]->node_idx() = i;
            }
        }
        /* get node from parent */
        values[0] = std::move(parent->value_at(parent_idx - 1)); // need destroy, cannot direct move construct
        keys[0] = std::move(parent->key_at(parent_idx - 1));     // therefore, move assignment is called
        usage++;

        /* update_parent */
        auto from_usage = from->node_usage();
        parent->value_at(parent_idx - 1) = std::move(from->value_at(from_usage - 1));
        parent->key_at(parent_idx - 1) = std::move(from->key_at(from_usage - 1));

        /* update from */
        std::destroy_at(&from->value_at(from_usage - 1));
        std::destroy_at(&from->key_at(from_usage - 1));
        from->node_usage() -= 1;

        /* take the child */
        if constexpr (IsInternal) {
            children[0] = from->child_at(from_usage);
            from->child_at(from_usage) = nullptr;
            children[0]->node_idx() = 0;
            children[0]->node_parent() = this;
        }
    }

    void borrow_right(NodePtr from) {
        ASSERT(dynamic_cast<BTreeNode *>(from));
        ASSERT(parent); // root will never borrow
        ASSERT(parent == dynamic_cast<BTreeNode *>(from)->parent);
        ASSERT(parent_idx == 0); // only the first element call this
        ASSERT(parent_idx < parent->node_usage());
        ASSERT(parent_idx == from->node_idx() - 1);
        ASSERT(from->node_usage() - 1 >= B - 1);
        ASSERT(usage + 1 >= B - 1);

        auto from_node = static_cast<BTreeNode *>(from);
        /* update this node */
        new(values + usage) V(std::move(parent->value_at(parent_idx)));
        new(keys + usage) K(
                std::move(parent->key_at(parent_idx))); // last element is uninitilized, direct move construct
        if constexpr (IsInternal) {
            children[usage + 1] = from_node->children[0];
            children[usage + 1]->node_parent() = this;
            children[usage + 1]->node_idx() = usage + 1;
        }
        usage++;

        /* update parent */
        parent->value_at(parent_idx) = std::move(from_node->values[0]);
        parent->key_at(parent_idx) = std::move(from_node->values[0]);

        /* update from node */
        std::move(from_node->keys + 1, from_node->keys + from_node->usage, from_node->keys);
        std::move(from_node->values + 1, from_node->values + from_node->usage, from_node->values);
        // memcpy should be good? but standards said UB if overlapped
        if constexpr (IsInternal) {
            std::memmove(from_node->children, from_node->children + 1, from_node->usage * sizeof(NodePtr));
            from_node->children[from_node->usage] = nullptr;
            for (auto i = 0; i < from_node->usage; ++i) {
                from_node->children[i]->node_idx() = i;
            }
        }
        std::destroy_at(from_node->values + usage - 1);
        std::destroy_at(from_node->keys + usage - 1);
        from_node->usage -= 1;
    }

    static void merge(NodePtr a, NodePtr b, NodePtr *root) {
        auto left = static_cast<BTreeNode *>(a);
        auto right = static_cast<BTreeNode *>(b);
        auto parent = static_cast<BTreeNode<K, V, true, Compare> *>(left->parent);
        ASSERT(dynamic_cast<BTreeNode *>(a));
        ASSERT(dynamic_cast<BTreeNode *>(b));
        ASSERT((dynamic_cast <BTreeNode<K, V, true, Compare> *> (left->parent))); // root will never borrow
        ASSERT(left->parent == right->parent);
        ASSERT(left->parent_idx == right->node_idx() - 1);
        ASSERT(left->usage + right->usage + 1 < 2 * B - 1);

        new(left->values + left->usage) V(std::move(parent->values[left->parent_idx]));
        new(left->keys + left->usage) K(std::move(parent->keys[left->parent_idx]));
        std::move(parent->values + right->parent_idx, parent->values + parent->usage,
                  parent->values + left->parent_idx);
        std::move(parent->keys + right->parent_idx, parent->keys + parent->usage, parent->keys + left->parent_idx);
        std::memmove(parent->children + right->parent_idx, parent->children + right->parent_idx + 1,
                     (parent->usage - right->parent_idx) * sizeof(NodePtr));
        parent->children[parent->usage--] = nullptr;
        for (auto i = right->parent_idx; i <= parent->usage; ++i) {
            parent->children[i]->node_idx() = i;
        }
        std::destroy_at(parent->keys + parent->usage);
        std::destroy_at(parent->values + parent->usage);

        left->usage++;
        std::uninitialized_move(right->values, right->values + right->usage, left->values + left->usage);
        std::uninitialized_move(right->keys, right->keys + right->usage, left->keys + left->usage);
        if constexpr (IsInternal) {
            std::memcpy(left->children + left->usage, right->children, (right->usage + 1) * sizeof(NodePtr));
            for (auto i = left->usage; i <= left->usage + right->usage; ++i) {
                left->children[i]->node_idx() = i;
                left->children[i]->node_parent() = left;
            }
        }
        left->usage += right->usage;

        right->usage = 0;
        delete right;

        if (parent->usage == 0) /* only possible at root or B == 2 */ {
            ASSERT(parent == *root );
            parent->usage = 0;
            left->parent = nullptr;
            *root = left;
            return;
        }

        parent->fix_underflow(root);
    }

    void fix_underflow(NodePtr *root) override {
        if (usage >= B - 1 || !parent) return;
        if (parent_idx) {
            auto target = parent->child_at(parent_idx - 1);
            if (target->node_usage() > B - 1) {
                borrow_left(target);
            } else {
                merge(target, this, root);
            }
        } else {
            auto target = parent->child_at(parent_idx + 1);
            if (target->node_usage() > B - 1) {
                borrow_right(target);
            } else {
                merge(this, target, root);
            }
        }
    }

#ifdef DEBUG_MODE

    void display(size_t ident) override {
        std::string idents(ident ? ident - 1 : 0, '-');
        if (ident) idents.push_back('>');
        if (ident) idents.push_back(' ');
        std::cout << idents << "node at " << this << ", parent: " << parent << ", index: " << parent_idx
                  << ", fields: ";
        {
            auto i = 0;
            for (; i < usage; ++i) {
                std::cout << " " << std::setw(4) << keys[i];
            }
            for (; i < 2 * B - 2; ++i) {
                std::cout << " " << std::setw(4) << "_";
            }
        }
        std::cout << std::endl;
        if constexpr (IsInternal) {
            for (auto i = 0; i <= usage; ++i) {
                children[i]->display(ident + 4);;
            }
        }
    }

#endif

    typename Node::iterator min() {
        if constexpr(IsInternal) {
            return children[0]->min();
        } else {
            return typename Node::iterator{
                    .idx = 0,
                    .node = this
            };
        }
    }

    typename Node::iterator max() {
        if constexpr(IsInternal) {
            return children[usage]->max();
        } else {
            return typename Node::iterator{
                    .idx = uint16_t(usage - 1u),
                    .node = this
            };
        }
    }

    NodePtr &child_at(size_t i) override {
        return children[i];
    }

    K &key_at(size_t i) override {
        return keys[i];
    }

    V &value_at(size_t i) override {
        return values[i];
    }

    typename Node::iterator predecessor(uint16_t idx) override {
        if constexpr (IsInternal) {
            return children[idx]->max();
        } else {
            if (idx)
                return typename Node::iterator{
                        .idx = uint16_t(idx - 1u),
                        .node = this
                };
            else {
                NodePtr node = this;
                while (node->node_parent() && node->node_idx() == 0) {
                    node = node->node_parent();
                }
                if (node->node_parent()) {
                    return typename Node::iterator{
                            .idx = uint16_t(node->node_idx() - 1u),
                            .node = node->node_parent()
                    };
                } else {
                    return typename Node::iterator{
                            .idx = 0,
                            .node = nullptr
                    };
                }
            }
        }
    }

    typename Node::iterator successor(uint16_t idx) override {
        if constexpr (IsInternal) {
            return children[idx + 1]->min();
        } else {
            if (idx < usage - 1)
                return typename Node::iterator{
                        .idx = uint16_t(idx + 1u),
                        .node = this
                };
            else {
                NodePtr node = this;
                while (node->node_parent() && node->node_idx() == node->node_parent()->node_usage()) {
                    node = node->node_parent();
                }
                if (node->node_parent()) {
                    return typename Node::iterator{
                            .idx = node->node_idx(),
                            .node = node->node_parent()
                    };
                } else {
                    return typename Node::iterator{
                            .idx = 0,
                            .node = nullptr
                    };
                }
            }
        }
    }

    ~BTreeNode() override {
        if constexpr(IsInternal) {
            if (usage)
                for (auto i = 0; i <= usage; ++i) {
                    delete children[i];
                }
        }
    }

    std::pair<K, V> erase(uint16_t index, NodePtr *root) override {
        if constexpr (IsInternal) {
            auto pred = children[index]->max();
            std::swap(keys[index], pred.node->key_at(pred.idx));
            std::swap(values[index], pred.node->value_at(pred.idx));
            return pred.node->erase(pred.idx, root);
        } else {
            std::pair<K, V> result(std::move(keys[index]), std::move(values[index]));
            std::move(keys + index + 1, keys + usage, keys + index);
            std::move(values + index + 1, values + usage, values + index);
            usage--;
            std::destroy_at(keys + usage);
            std::destroy_at(values + usage);
            fix_underflow(root);
            return result;
        }
    }
};

template<class K, class V, size_t H>
using DefaultBTNode = BTreeNode<K, V, H>;

template<typename K, typename V, size_t B, typename Compare>
Compare AbstractBTNode<K, V, B, Compare>::comp{};

#endif

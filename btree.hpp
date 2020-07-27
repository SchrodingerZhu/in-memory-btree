#ifndef BTREE_HPP
#define BTREE_HPP

#include <algorithm>
#include <cstring>

#define keys node_keys()
#define values node_values()
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
#include <libunwind.h>
#include <cxxabi.h>
#include <cstdio>

#define __TOKEN(x) #x
#define __STR(x) __TOKEN(x)
#define ASSERT(x) if (!(x)) { std::cerr << "assertion failed: " << __STR(x) << std::endl; backtrace(); }

void backtrace() {
    unw_cursor_t cursor;
    unw_context_t context;

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    // Unwind frames one by one, going up the frame stack.
    while (unw_step(&cursor) > 0) {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
            break;
        }
        std::printf("0x%lx:", pc);

        char sym[256];
        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
            char *nameptr = sym;
            int status;
            char *demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &status);
            if (status == 0) {
                nameptr = demangled;
            }
            std::printf(" (%s+0x%lx)\n", nameptr, offset);
            std::free(demangled);
        } else {
            std::printf(" -- error: unable to obtain symbol name for this frame\n");
        }
    }
    std::abort();
}

#else
#define ASSERT(x)
#endif

#ifdef DEBUG_MODE
static size_t alive_node = 0;
#endif

namespace btree {

    template<typename K, typename V, bool UseBinary = true, size_t B = DEFAULT_BTREE_FACTOR, typename Compare = std::less<K>>
    class BTree;

    namespace __btree_impl {

        template<typename K, typename V, bool UseBinary = true, size_t B = DEFAULT_BTREE_FACTOR, typename Compare = std::less<K>>
        struct AbstractBTNode;

        template<typename K, typename V, bool IsInternal, bool UseBinary = true, typename Compare = std::less<K>, size_t B = DEFAULT_BTREE_FACTOR>
        struct alignas(64) BTreeNode;

        template<typename T>
        inline void uninitialized_move_back(T *start, T *end) {
            ASSERT(end >= start);
            if constexpr (std::is_trivial_v<T>) {
                std::memmove(start + 1, start, (end - start) * sizeof(T));
            } else {
                for (auto i = end - 1; i >= start; --i) {
                    new(i + 1) T(std::move(*i));
                    std::destroy_at(i);
                }
            }
        }

        template<typename T>
        inline void uninitialized_move_forward(T *start, T *end) {
            ASSERT(end >= start);
            if constexpr (std::is_trivial_v<T>) {
                std::memmove(start - 1, start, (end - start) * sizeof(T));
            } else {
                for (auto i = start; i < end; ++i) {
                    new(i - 1) T(std::move(*i));
                    std::destroy_at(i);
                }
            }
        }

        template<typename K, typename V, bool UseBinary, size_t B, typename Compare>
        struct AbstractBTNode {

            Compare &comp;

            struct SplitResult {
                AbstractBTNode *l, *r;
                K key;
                V value;
            };

            struct iterator {
                uint16_t idx;
                AbstractBTNode *node;

                inline bool operator!=(const iterator &that) noexcept {
                    return idx != that.idx || node != that.node;
                }

#ifndef __cpp_lib_three_way_comparison
                inline bool operator==(const iterator &that) noexcept {
                    return idx == that.idx && node == that.node;
                }
#endif

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

            AbstractBTNode(Compare &comp) : comp(comp) {}

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

            virtual K *node_keys() = 0;

            virtual V *node_values() = 0;

            virtual std::pair<K, V> erase(uint16_t index, AbstractBTNode **root) = 0;

            virtual AbstractBTNode *&child_at(size_t) = 0;

            virtual void traversal_copy(AbstractBTNode *now, Compare &new_comp) = 0;

            virtual AbstractBTNode *same_type(Compare &new_comp) = 0;

            virtual ~AbstractBTNode() = default;

#ifdef DEBUG_MODE

            virtual void display(size_t indent) = 0;

#endif

            friend BTree<K, V, UseBinary, B, Compare>;
        };

        template<typename K, typename V, bool IsInternal, bool UseBinary, typename Compare, size_t B>
        struct alignas(64) BTreeNode : AbstractBTNode<K, V, UseBinary, B, Compare> {
            static_assert(2 * B < FOUND, "B is too large");
            static_assert(B > 2, "B is too small");
            using Node = AbstractBTNode<K, V, UseBinary, B, Compare>;
            using NodePtr = Node *;
            using SplitResult = typename Node::SplitResult;
            using KeyBlock = std::aligned_storage_t<sizeof(K), alignof(K)>;
            using ValueBlock = std::aligned_storage_t<sizeof(V), alignof(V)>;

            KeyBlock __keys[2 * B - 1];
            ValueBlock __values[2 * B - 1];

            NodePtr children[IsInternal ? (2 * B) : 0];
            NodePtr parent = nullptr;
            uint16_t usage = 0;
            uint16_t parent_idx = 0;

            using LocFlag = uint;

            BTreeNode(Compare &comp) : Node(comp) {
#ifdef DEBUG_MODE
                alive_node++;
#endif
                std::memset(__keys, 0, sizeof(__keys));
                std::memset(__values, 0, sizeof(__values));
            }

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
                if constexpr (UseBinary) {
                    uint16_t position = std::lower_bound(keys, keys + usage, key, this->comp) - keys;
                    if (position != usage && !Node::comp(key, keys[position])) {
                        return FOUND | position;
                    }
                    return GO_DOWN | position;
                } else {
                    uint i = 0;
                    for (; i < usage && this->comp(keys[i], key); ++i);
                    if (i == usage) return GO_DOWN | usage;
                    if (this->comp(key, keys[i])) {
                        return GO_DOWN | i;
                    }
                    return FOUND | i;
                }
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
                auto l = new BTreeNode(this->comp);
                auto r = new BTreeNode(this->comp);
                l->usage = r->usage = B - 1;
                l->parent = r->parent = this->parent;
                std::uninitialized_move(keys, keys + B - 1, l->keys);
                std::uninitialized_move(keys + B, keys + usage, r->keys);
                std::uninitialized_move(values, values + B - 1, l->values);
                std::uninitialized_move(values + B, values + usage, r->values);
                auto result = SplitResult{
                        .l = l,
                        .r = r,
                        .key = std::move(keys[B - 1]),
                        .value = std::move(values[B - 1]),
                };
                std::destroy(keys, keys + usage);
                std::destroy(values, values + usage);
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
                return result;
            }

            inline K *node_keys() override {
                return reinterpret_cast<K *>(__keys);
            };

            inline V *node_values() override {
                return reinterpret_cast<V *>(__values);
            };

            inline NodePtr same_type(Compare &new_comp) override {
                return new BTreeNode(new_comp);
            };

            inline void traversal_moveup(NodePtr now, Compare &new_comp) {
                std::uninitialized_copy(keys, keys + usage, now->keys);
                std::uninitialized_copy(values, values + usage, now->values);
                now->node_usage() = usage;
                now->node_idx() = parent_idx;
                if (parent == nullptr) { return; }
                auto new_parent = now->node_parent();
                if (new_parent == nullptr) {
                    ASSERT(parent_idx == 0);
                    new_parent = new BTreeNode<K, V, true, UseBinary, Compare, B>(new_comp);
                    new_parent->child_at(0) = now;
                    now->node_parent() = new_parent;
                }
                new_parent->child_at(new_parent->node_usage()) = now;
                new_parent->node_usage() += 1;
                return parent->traversal_copy(new_parent, new_comp);
            }

            void traversal_copy(NodePtr now, Compare &new_comp) override {
                ASSERT(dynamic_cast<BTreeNode *>(now)); // must of the same type
                if constexpr (IsInternal) {
                    if (usage + 1 == now->node_usage()) {
                        return traversal_moveup(now, new_comp);
                    }
                    auto child = children[now->node_usage()]->same_type(new_comp);
                    child->node_parent() = now;
                    return children[now->node_usage()]->traversal_copy(child, new_comp);
                } else {
                    return traversal_moveup(now, new_comp);
                }
            };

            static NodePtr singleton(NodePtr l, NodePtr r, K key, V value, Compare &_comp) {
                auto node = new BTreeNode<K, V, true, UseBinary, Compare, B>(_comp);
                node->usage = 1;
                new(node->__values) V(std::move(value));  // no need for destroy, directly move
                new(node->__keys) K(std::move(key));
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
                    std::destroy_at(values + (res & FOUND_MASK));
                    new(values + (res & FOUND_MASK)) V(value);
                    return {original};
                }
                auto position = res & GO_DOWN_MASK;
                if constexpr (IsInternal) {
                    return children[position]->insert(key, value, root);
                } else {
                    uninitialized_move_back(values + position, values + usage);
                    uninitialized_move_back(keys + position, keys + usage);
                    new(values + position) V(value);
                    new(keys + position) K(key);
                    usage++;
                    if (usage == 2 * B - 1) /* leaf if full */ {
                        auto result = split();
                        if (parent)
                            parent->adopt(result.l, result.r, std::move(result.key), std::move(result.value),
                                          parent_idx, root);
                        else {
                            auto node = singleton(result.l, result.r, std::move(result.key), std::move(result.value),
                                                  this->comp);
                            delete *root;
                            *root = node;
                        }
                    }
                    return std::nullopt;
                }
            }

            void adopt(NodePtr l, NodePtr r, K key, V value, size_t position, NodePtr *root) override {
                uninitialized_move_back(values + position, values + usage);
                uninitialized_move_back(keys + position, keys + usage);
                if constexpr (IsInternal) {
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
                }
                new(values + position) V(std::move(value));
                new(keys + position) K(std::move(key));
                usage++;
                if (usage == 2 * B - 1) {
                    auto result = split();
                    if (parent) {
                        parent->adopt(result.l, result.r, std::move(result.key), std::move(result.value), parent_idx,
                                      root);
                    } else {
                        auto node = singleton(result.l, result.r, std::move(result.key), std::move(result.value),
                                              this->comp);
                        delete *root;
                        *root = node;
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

                uninitialized_move_back(keys, keys + usage);
                uninitialized_move_back(values, values + usage);

                /* get node from parent */
                new(values) V(std::move(parent->value_at(parent_idx - 1)));
                new(keys) K(std::move(parent->key_at(parent_idx - 1)));
                std::destroy_at(parent->values + parent_idx - 1);
                std::destroy_at(parent->keys + parent_idx - 1);
                usage++;

                /* update_parent */
                auto from_usage = from->node_usage();
                new(parent->values + parent_idx - 1) V(std::move(from->value_at(from_usage - 1)));
                new(parent->keys + parent_idx - 1) K(std::move(from->key_at(from_usage - 1)));
                /* update from */
                std::destroy_at(from->values + (from_usage - 1));
                std::destroy_at(from->keys + (from_usage - 1));
                from->node_usage() -= 1;

                /* take the child */
                if constexpr (IsInternal) {
                    std::memmove(children + 1, children, usage * sizeof(NodePtr));
                    children[0] = from->child_at(from_usage);
                    from->child_at(from_usage) = nullptr;
                    children[0]->node_idx() = 0;
                    children[0]->node_parent() = this;
                    for (auto i = 1; i <= usage; ++i) {
                        children[i]->node_idx() = i;
                    }
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
                        std::move(parent->key_at(parent_idx))); // last element is uninitialized, direct move construct
                std::destroy_at(parent->values + parent_idx);
                std::destroy_at(parent->keys + parent_idx);
                if constexpr (IsInternal) {
                    children[usage + 1] = from_node->children[0];
                    children[usage + 1]->node_parent() = this;
                    children[usage + 1]->node_idx() = usage + 1;
                }
                usage++;

                /* update parent */
                new(parent->values + parent_idx) V(std::move(from_node->values[0]));
                new(parent->keys + parent_idx) K(std::move(from_node->keys[0]));
                std::destroy_at(from_node->values);
                std::destroy_at(from_node->keys);

                /* update from node */
                uninitialized_move_forward(from_node->keys + 1, from_node->keys + from_node->usage);
                uninitialized_move_forward(from_node->values + 1, from_node->values + from_node->usage);

                // memcpy should be good? but standards said UB if overlapped
                if constexpr (IsInternal) {
                    std::memmove(from_node->children, from_node->children + 1, from_node->usage * sizeof(NodePtr));
                    from_node->children[from_node->usage] = nullptr;
                    for (auto i = 0; i < from_node->usage; ++i) {
                        from_node->children[i]->node_idx() = i;
                    }
                }
                from_node->usage -= 1;
            }

            static void merge(NodePtr a, NodePtr b, NodePtr *root) {
                auto left = static_cast<BTreeNode *>(a);
                auto right = static_cast<BTreeNode *>(b);
                auto parent = static_cast<BTreeNode<K, V, true, UseBinary, Compare> *>(left->parent);
                ASSERT(dynamic_cast<BTreeNode *>(a));
                ASSERT(dynamic_cast<BTreeNode *>(b));
                ASSERT((dynamic_cast <BTreeNode<K, V, true, UseBinary, Compare> *> (left->parent))); // root will never borrow
                ASSERT(left->parent == right->parent);
                ASSERT(left->parent_idx == right->node_idx() - 1);
                ASSERT(left->usage + right->usage + 1 < 2 * B - 1);

                new(left->values + left->usage) V(std::move(parent->values[left->parent_idx]));
                new(left->keys + left->usage) K(std::move(parent->keys[left->parent_idx]));
                std::destroy_at(parent->values + left->parent_idx);
                std::destroy_at(parent->keys + left->parent_idx);
                uninitialized_move_forward(parent->values + right->parent_idx, parent->values + parent->usage);
                uninitialized_move_forward(parent->keys + right->parent_idx, parent->keys + parent->usage);
                std::memmove(parent->children + right->parent_idx, parent->children + right->parent_idx + 1,
                             (parent->usage - right->parent_idx) * sizeof(NodePtr));
                parent->children[parent->usage--] = nullptr;


                left->usage++;
                std::uninitialized_move(right->values, right->values + right->usage, left->values + left->usage);
                std::uninitialized_move(right->keys, right->keys + right->usage, left->keys + left->usage);
                std::destroy(right->values, right->values + right->usage);
                std::destroy(right->keys, right->keys + right->usage);

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

                for (auto i = left->parent_idx; i <= parent->usage; ++i) {
                    parent->children[i]->node_idx() = i;
                }

                if (parent->usage == 0) /* only possible at root or B == 2 */ {
                    ASSERT(parent == *root);
                    parent->usage = 0;
                    delete (parent);
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
                {
                    unsigned i = 0;
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

            typename Node::iterator min() override {
                if constexpr(IsInternal) {
                    return children[0]->min();
                } else {
                    return typename Node::iterator{
                            .idx = 0,
                            .node = this
                    };
                }
            }

            typename Node::iterator max() override {
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
                if constexpr (IsInternal) {
                    return children[i];
                } else {
                    std::abort();
                }
            }

            inline K &key_at(size_t i) override {
                return keys[i];
            }

            inline V &value_at(size_t i) override {
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
#ifdef DEBUG_MODE
                alive_node--;
#endif
                std::destroy(keys, keys + usage);
                std::destroy(values, values + usage);
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
                    std::destroy_at(keys + index);
                    std::destroy_at(values + index);
                    uninitialized_move_forward(keys + index + 1, keys + usage);
                    uninitialized_move_forward(values + index + 1, values + usage);
                    usage--;
                    fix_underflow(root);
                    return result;
                }
            }
        };

    }

    template<typename K, typename V, bool UseBinary, size_t B, typename Compare>
    class BTree {

        using Node = __btree_impl::AbstractBTNode<K, V, UseBinary, B, Compare>;
        size_t _size = 0;
        Node *root = nullptr;

        Compare comp;
    public:

        BTree(Compare comp = Compare()) : comp(comp) {}

        BTree(BTree &&that) noexcept(Compare(std::move(comp))) {
            root = that.root;
            _size = that._size;
            comp = std::move(that.comp);
        }

        BTree(const BTree &that) {
            comp = that.comp;
            _size = that._size;
            if (that.root == nullptr) {
                root = nullptr;
                return;
            } else {
                root = that.root->same_type(comp);
                that.root->traversal_copy(root, comp);
            }
        }

        using iterator = typename Node::iterator;
#ifdef DEBUG_MODE

        void display() {
            if (root) root->display(0);
        };
#endif

        std::optional<V> insert(const K &key, const V &value) {
            if (root == nullptr) {
                auto node = new __btree_impl::BTreeNode<K, V, false, UseBinary, Compare, B>(comp);
                node->usage = 1;
                new(node->__keys) K(key);
                new(node->__values) V(value);
                root = node;
                _size++;
                return std::nullopt;
            }
            auto res = root->insert(key, value, &root);
            if (!res) _size++;
            return res;
        }

        bool empty() {
            return _size == 0;
        }

        bool member(const K &key) {
            return root && root->member(key);
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
            if (_size)
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
            _size--;
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

        size_t size() {
            return _size;
        }
    };
}

#undef keys
#undef values
#undef GO_DOWN
#undef GO_DOWN_MASK
#undef FOUND
#undef FOUND_MASK
#endif // BTREE_HPP

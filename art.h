#pragma once

#include <climits>
#include <string>
#include <utility>

template <std::size_t N, typename... T> struct typelist_find_helper;
template <std::size_t N, typename T> struct typelist_find_helper<N, T> {
  template <typename Target> constexpr static std::size_t find() {
    if (std::is_same<Target, T>::value) {
      return N;
    }
    return -1;
  }
};
template <std::size_t N, typename T, typename... Rest>
struct typelist_find_helper<N, T, Rest...> {
  template <typename Target> constexpr static std::size_t find() {
    if (std::is_same<Target, T>::value) {
      return N;
    }
    return typelist_find_helper<N + 1, Rest...>::template find<Target>();
  }
};

template <std::size_t N, typename... T> struct typelist_get_helper;
template <typename T, typename... Rest>
struct typelist_get_helper<0, T, Rest...> {
  using type = T;
};
template <std::size_t N, typename T, typename... Rest>
struct typelist_get_helper<N, T, Rest...>
    : public typelist_get_helper<N - 1, Rest...> {};

template <typename... T> struct typelist {
  constexpr static std::size_t size = sizeof...(T);

  template <typename R> constexpr static std::size_t find() {
    return typelist_find_helper<0, T...>::template find<R>();
  }

  template <std::size_t N>
  using get_type = typename typelist_get_helper<N, T...>::type;
};

template <template <typename> class R, typename T>
struct type_list_apply_helper;
template <template <typename> class R, typename... T>
struct type_list_apply_helper<R, typelist<T...>> {
  using type = typelist<R<T>...>;
};

template <template <typename> class R, typename Tlist> struct type_list_apply {
  using type = typename type_list_apply_helper<R, Tlist>::type;
};

template <typename Tlist> struct chain_derived_typelist_container;
template <typename T, typename... R>
struct chain_derived_typelist_container<typelist<T, R...>> {
  struct container
      : public T,
        public chain_derived_typelist_container<typelist<R...>>::type {
    template <typename... Args>
    container(Args &&...args)
        : T(std::forward<Args>(args)...),
          chain_derived_typelist_container<typelist<R...>>::type(
              std::forward<Args>(args)...) {}
  };
  using type = container;
};
template <typename T> struct chain_derived_typelist_container<typelist<T>> {
  struct container : public T {
    using T::T;
  };
  using type = container;
};

using char_type = char;

constexpr char_type char_type_minium = CHAR_MIN;
constexpr char_type char_type_maxium = CHAR_MAX;

template <typename V> struct node_base;
template <typename V> struct node0;
template <typename V> struct node4;
template <typename V> struct node16;
template <typename V> struct node48;
template <typename V> struct node256;

template <typename V> using node_type_guard = node0<V>;

template <typename V>
using levellist =
    typelist<node4<V>, node16<V>, node48<V>, node256<V>, node_type_guard<V>>;

template <typename K, typename V, typename Alloc> struct art_tree;

template <typename V> struct child_slot {
  char_type c;
  node_base<V> **node;
};

template <typename V> struct const_child_slot {
  char_type c;
  node_base<V> *const *node;
};

enum bound_direction { lower, upper };

template <typename V> struct find_result_type {
  node_base<V> *node;        // current node
  child_slot<V> parent_slot; // parent child slot
  size_t key_cur;      // diff cursor of key, equal to key size when key match.
  size_t node_sub_cur; // diff cursor of node subfix
};

struct node_link_base {
  node_link_base *prev_;
  node_link_base *next_;
};

template <typename V> struct node_base : public node_link_base {
  using key_type =
      typename std::remove_const<typename std::tuple_element<0, V>::type>::type;
  using mapped_type = typename std::tuple_element<1, V>::type;
  using value_type = V;

  node_base() = default;
  virtual ~node_base() = default;

  virtual std::size_t node_size() const = 0;
  virtual node_base<value_type> *expand_new(void *art_tree_ptr) = 0;
  virtual void dealloc(void *art_tree_ptr) = 0;

  virtual const_child_slot<value_type> find_child_impl(char_type c) const = 0;
  virtual child_slot<value_type> find_leq_child(char_type c) = 0;
  virtual child_slot<value_type> find_geq_child(char_type c) = 0;
  virtual int get_all_children_impl(child_slot<value_type> slots[256]) = 0;
  virtual std::pair<child_slot<value_type>, bool>
  try_insert_child_impl(char_type c, node_base *node) = 0;
  virtual void erase_child(char_type c) = 0;

  bool children_empty() const { return children_size_ == 0; }

  int get_all_children(child_slot<value_type> slots[256]) {
    if (children_empty()) {
      return 0;
    }
    return get_all_children_impl(slots);
  }
  const_child_slot<value_type> find_child(char_type c) const {
    const_child_slot<value_type> cslot;
    if (children_empty()) {
      cslot.node = nullptr;
      return cslot;
    }
    return find_child_impl(c);
  }
  child_slot<value_type> find_child(char_type c) {
    const_child_slot<value_type> cslot =
        const_cast<const node_base *>(this)->find_child(c);
    child_slot<value_type> slot;
    slot.c = cslot.c;
    slot.node = const_cast<node_base **>(cslot.node);
    return slot;
  }
  child_slot<value_type> find_less_child(char_type c) {
    child_slot<value_type> slot;
    if (c == char_type_minium) {
      slot.node = nullptr;
      return slot;
    }
    return find_leq_child(c - 1);
  }
  child_slot<value_type> find_greater_child(char_type c) {
    child_slot<value_type> slot;
    if (c == char_type_maxium) {
      slot.node = nullptr;
      return slot;
    }
    return find_geq_child(c + 1);
  }
  child_slot<value_type> find_min_child() {
    return find_geq_child(char_type_minium);
  }
  child_slot<value_type> find_max_child() {
    return find_leq_child(char_type_maxium);
  }

  void set_node_value(const value_type &value) {
    value_storage_.first = value.first;
    new (&value_storage_.second) mapped_type(value.second);
    storage_valid_ = true;
  }
  void move_node_value(value_type &&value) {
    value_storage_.first = std::forward<const key_type>(value.first);
    new (&value_storage_.second)
        mapped_type(std::forward<mapped_type>(value.second));
    storage_valid_ = true;
  }
  void unset_node_value() {
    get_value().second.~mapped_type();
    storage_valid_ = false;
  }
  value_type &get_value() {
    if (!storage_valid_) {
      throw "get value";
    }
    return *reinterpret_cast<value_type *>(&value_storage_);
  }
  const value_type &get_value() const {
    if (!storage_valid_) {
      throw "get value";
    }
    return *reinterpret_cast<const value_type *>(&value_storage_);
  }

  // if node's storage will be valid, set value before calling this function.
  void set_node_subfix(const char_type *subfix, std::size_t subfix_size) {
    if (!storage_valid_) {
      // the key of storage is just subfix, not full key
      value_storage_.first = key_type(subfix, subfix_size);
    }
    subfix_start_ = const_cast<char_type *>(value_storage_.first.c_str()) +
                    value_storage_.first.size() - subfix_size;
    subfix_size_ = subfix_size;
  }
  void truncate_node_prefix(std::size_t truncate_size) {
    subfix_size_ -= truncate_size;
    subfix_start_ += truncate_size;
  }
  std::pair<std::size_t, int> compare(const char_type *s,
                                      std::size_t ssize) const {
    const std::size_t ds = std::min(subfix_size_, ssize);
    std::size_t i = 0;

    // loop unfold
#define DO(n)                                                                  \
  do {                                                                         \
    const std::size_t p = i + n;                                               \
    const char_type d = subfix_start_[p] - s[p];                               \
    if (d != 0) {                                                              \
      return {p, static_cast<int>(d)};                                         \
    }                                                                          \
                                                                               \
  } while (0)

    while (i < ds % 8) {
      DO(0);
      ++i;
    }

    while (i < ds) {
      DO(0);
      DO(1);
      DO(2);
      DO(3);
      DO(4);
      DO(5);
      DO(6);
      DO(7);
      i += 8;
    }

#undef DO

    return {ds, subfix_size_ - ssize};
  }

  node_base *find_min_data_node() {
    node_base *node = this;
    while (!node->storage_valid_) {
      child_slot<value_type> slot = node->find_min_child();
      node = *slot.node;
    }

    if (!node->storage_valid_) {
      throw "bad found min data node";
    }

    return node;
  }
  node_base *find_max_data_node() {
    node_base *node = this;
    while (true) {
      child_slot<value_type> slot = node->find_max_child();
      if (slot.node == nullptr) {
        break;
      }
      node = *slot.node;
    }

    if (!node->storage_valid_) {
      throw "bad found max data node";
    }

    return node;
  }

  std::pair<child_slot<value_type>, bool> try_insert_child(char_type c,
                                                           node_base *node) {
    auto r = try_insert_child_impl(c, node);
    if (r.second) {
      node->parent_ = this;
      node->parent_c_ = c;
    }
    return r;
  }

  std::pair<key_type,
            typename std::aligned_storage<sizeof(mapped_type), 8>::type>
      value_storage_;
  node_base *parent_;
  std::size_t subfix_size_;
  char_type *subfix_start_;
  uint16_t children_size_;
  bool storage_valid_;
  char_type parent_c_;
};

template <typename V> struct node0 : public node_base<V> {
  std::size_t node_size() const override { throw "not implement"; }
  const_child_slot<V> find_child_impl(char_type c) const override {
    throw "not implement";
  }
  child_slot<V> find_leq_child(char_type c) override { throw "not implement"; }
  child_slot<V> find_geq_child(char_type c) override { throw "not implement"; }
  int get_all_children_impl(child_slot<V> slots[256]) override {
    throw "not implement";
  }
  std::pair<child_slot<V>, bool>
  try_insert_child_impl(char_type c, node_base<V> *node) override {
    throw "not implement";
  }
  void erase_child(char_type c) override { throw "not implement"; }
};

template <typename V> struct node4 : public node_base<V> {
  std::size_t node_size() const override;
  const_child_slot<V> find_child_impl(char_type c) const override;
  child_slot<V> find_leq_child(char_type c) override;
  child_slot<V> find_geq_child(char_type c) override;
  int get_all_children_impl(child_slot<V> slots[256]) override;
  std::pair<child_slot<V>, bool>
  try_insert_child_impl(char_type c, node_base<V> *node) override;
  void erase_child(char_type c) override;

  char_type keys_[4];
  node_base<V> *children_[4];

  constexpr static int max_children_size = 4;
};

template <typename V> struct node16 : public node_base<V> {
  std::size_t node_size() const override;
  const_child_slot<V> find_child_impl(char_type c) const override;
  child_slot<V> find_leq_child(char_type c) override;
  child_slot<V> find_geq_child(char_type c) override;
  int get_all_children_impl(child_slot<V> slots[256]) override;
  std::pair<child_slot<V>, bool>
  try_insert_child_impl(char_type c, node_base<V> *node) override;
  void erase_child(char_type c) override;

  char_type keys_[16];
  node_base<V> *children_[16];

  constexpr static int max_children_size = 16;
};

template <typename V> struct node48 : public node_base<V> {
  uint8_t *children_index() { return children_index_ - char_type_minium; }
  const uint8_t *children_index() const {
    return children_index_ - char_type_minium;
  }

  std::size_t node_size() const override;
  const_child_slot<V> find_child_impl(char_type c) const override;
  child_slot<V> find_leq_child(char_type c) override;
  child_slot<V> find_geq_child(char_type c) override;
  int get_all_children_impl(child_slot<V> slots[256]) override;
  std::pair<child_slot<V>, bool>
  try_insert_child_impl(char_type c, node_base<V> *node) override;
  void erase_child(char_type c) override;

  uint8_t children_index_[256];
  node_base<V> *children_[49]; // chilren_[0] always nullptr

  constexpr static int max_children_size = 48;
};

template <typename V> struct node256 : public node_base<V> {
  node_base<V> **children() { return children_ - char_type_minium; }
  node_base<V> *const *children() const { return children_ - char_type_minium; }

  std::size_t node_size() const override;
  const_child_slot<V> find_child_impl(char_type c) const override;
  child_slot<V> find_leq_child(char_type c) override;
  child_slot<V> find_geq_child(char_type c) override;
  int get_all_children_impl(child_slot<V> slots[256]) override;
  std::pair<child_slot<V>, bool>
  try_insert_child_impl(char_type c, node_base<V> *node) override;
  void erase_child(char_type c) override;

  node_base<V> *children_[256];

  constexpr static int max_children_size = 256;
};

template <typename K, typename V, typename Alloc> struct art_tree {
  using key_type = K;
  using mapped_type = typename std::tuple_element<1, V>::type;
  using value_type = std::pair<const key_type, mapped_type>;
  using allocator_type = Alloc;

  template <typename node_type, typename = void>
  struct node_alloca_helper : public node_type {
    node_base<value_type> *expand_new(void *art_tree_ptr) override {
      art_tree<key_type, value_type, Alloc> *art =
          static_cast<art_tree<key_type, value_type, Alloc> *>(art_tree_ptr);
      return art->template node_new<typename levellist<V>::template get_type<
          levellist<V>::template find<node_type>() + 1>>();
    }

    void dealloc(void *art_tree_ptr) override {
      using node_alloca_type = typename std::allocator_traits<
          Alloc>::template rebind_alloc<node_alloca_helper<node_type>>;

      art_tree<key_type, value_type, Alloc> *art =
          static_cast<art_tree<key_type, value_type, Alloc> *>(art_tree_ptr);
      node_alloca_helper::~node_alloca_helper();
      art->impl_.node_alloca_type::deallocate(this, 1);
    }
  };

  template <typename unused>
  struct node_alloca_helper<node_type_guard<value_type>, unused>
      : public node_type_guard<value_type> {
    node_base<value_type> *expand_new(void *art_tree_ptr) {
      throw "not implement";
    }
    void dealloc(void *art_tree_ptr) override { throw "not implement"; }
  };

  template <typename node_type>
  struct node_alloca_traits_rebind
      : public std::allocator_traits<Alloc>::template rebind_alloc<
            node_alloca_helper<node_type>> {
    using traits = typename std::allocator_traits<Alloc>::template rebind_alloc<
        node_alloca_helper<node_type>>;
    using traits::traits;
  };

  using node_allocator_traits =
      typename chain_derived_typelist_container<typename type_list_apply<
          node_alloca_traits_rebind, levellist<value_type>>::type>::type;

  art_tree(const allocator_type &alloc = allocator_type()) : impl_(alloc) {}

  template <typename node_type> node_type *node_new() {
    using node_allocator_type = typename std::allocator_traits<
        Alloc>::template rebind_alloc<node_alloca_helper<node_type>>;

    node_alloca_helper<node_type> *node =
        impl_.node_allocator_type::allocate(1);
    new (node) node_alloca_helper<node_type>();
    ++impl_.node_counter_;
    return node;
  }
  void node_delete(node_base<value_type> *node) {
    if (node->storage_valid_) {
      node->unset_node_value();
    }
    --impl_.node_counter_;
    node->dealloc(static_cast<void *>(this));
  }
  bool is_root(node_base<value_type> *node) { return impl_.root_ == node; }
  find_result_type<value_type> find_last_node(node_base<value_type> *start_node,
                                              const char_type *subfix,
                                              std::size_t subfix_size) const {
    find_result_type<value_type> result;
    if (start_node == nullptr) {
      result.node = nullptr;
      return result;
    }

    std::size_t cursor = 0;
    node_base<value_type> *node = start_node;
    child_slot<value_type> parent_slot;
    while (true) {
      if (cursor > subfix_size) {
        throw "find overflow";
      }

      auto r = node->compare(subfix + cursor, subfix_size - cursor);
      if (cursor + r.first < subfix_size && r.first == node->subfix_size_) {
        child_slot<value_type> slot =
            node->find_child(subfix[cursor + r.first]);
        if (slot.node != nullptr) {
          // find next child
          cursor += r.first + 1;
          node = *slot.node;
          parent_slot = slot;
          continue;
        }
      }
      result = {
          .node = node,
          .parent_slot = parent_slot,
          .key_cur = cursor + r.first,
          .node_sub_cur = r.first,
      };
      return result;
    }
  }
  void insert_node_link(node_link_base *node, node_link_base *bound_node,
                        bound_direction direction) {
    if (direction == lower) {
      node->next_ = bound_node->next_;
      node->prev_ = bound_node;
      bound_node->next_->prev_ = node;
      bound_node->next_ = node;
    } else if (direction == upper) {
      node->next_ = bound_node;
      node->prev_ = bound_node->prev_;
      bound_node->prev_->next_ = node;
      bound_node->prev_ = node;
    }
  }
  void replace_node_link(node_link_base *node, node_link_base *oldnode) {
    node->prev_ = oldnode->prev_;
    node->next_ = oldnode->next_;
    oldnode->prev_->next_ = node;
    oldnode->next_->prev_ = node;
  }
  void erase_node_link(node_link_base *node) {
    node->prev_->next_ = node->next_;
    node->next_->prev_ = node->prev_;
  }
  void change_node_parent_child(node_base<value_type> *node,
                                node_base<value_type> *oldnode,
                                node_base<value_type> **child_slot) {
    node->parent_ = oldnode->parent_;
    if (is_root(oldnode)) {
      impl_.root_ = node;
    } else {
      *child_slot = node;
      node->parent_c_ = oldnode->parent_c_;
    }
  }
  node_base<value_type> *node_expand(node_base<value_type> *node) {
    node_base<value_type> *expanded_node =
        node->expand_new(static_cast<void *>(this));
    child_slot<value_type> slots[256];
    int slot_size = node->get_all_children(slots);
    for (int i = 0; i < slot_size; ++i) {
      expanded_node->try_insert_child(slots[i].c, *slots[i].node);
    }
    if (node->storage_valid_) {
      expanded_node->move_node_value(std::move(node->get_value()));
      node->unset_node_value();
      replace_node_link(expanded_node, node);
    }
    expanded_node->set_node_subfix(node->subfix_start_, node->subfix_size_);
    return expanded_node;
  }
  void erase_node_with_one_child(node_base<value_type> *node,
                                 node_base<value_type> **child_slot_ptr) {
    child_slot<value_type> slot = node->find_min_child();
    node_base<value_type> *child = *slot.node;
    key_type new_child_subfix;
    new_child_subfix.reserve(node->subfix_size_ + 1 + child->subfix_size_);
    new_child_subfix.append(node->subfix_start_, node->subfix_size_);
    new_child_subfix.append(1, slot.c);
    new_child_subfix.append(child->subfix_start_, child->subfix_size_);
    child->set_node_subfix(new_child_subfix.c_str(), new_child_subfix.size());

    change_node_parent_child(child, node, child_slot_ptr);

    node_delete(node);
  }

  std::pair<node_base<value_type> *, bool> find(const key_type &_key) const {
    const std::size_t key_size = _key.size();
    const char_type *key = _key.c_str();

    find_result_type<value_type> find_result =
        find_last_node(impl_.root_, key, key_size);
    if (find_result.node && find_result.key_cur == key_size &&
        find_result.node_sub_cur == find_result.node->subfix_size_ &&
        find_result.node->storage_valid_) {
      return {find_result.node, true};
    }
    return {nullptr, false};
  }

  std::pair<node_base<value_type> *, bool> insert(const value_type &value) {
    const std::size_t key_size = value.first.size();
    const char_type *key = value.first.c_str();

    if (impl_.root_ == nullptr) {
      impl_.root_ = node_new<node4<value_type>>();
      impl_.root_->set_node_value(value);
      impl_.root_->set_node_subfix(key, key_size);
      insert_node_link(impl_.root_, &impl_.dummy_, lower);

      ++impl_.size_;
      return {impl_.root_, true};
    }

    const find_result_type<value_type> find_result =
        find_last_node(impl_.root_, key, key_size);
    node_base<value_type> *node = find_result.node;
    const char_type *subfix = key + find_result.key_cur;
    const std::size_t subfix_size = key_size - find_result.key_cur;

    if (find_result.node_sub_cur == node->subfix_size_ && subfix_size == 0) {
      // find this node
      if (node->storage_valid_) {
        // insert failed, because key is existed
        return {node, false};
      }
      node->set_node_value(value);
      // here need reset subfix start
      node->set_node_subfix(node->subfix_start_, node->subfix_size_);

      // this node is no data before, so it must has children. The key of
      // the child is greater than this node.
      insert_node_link(
          node, (*node->find_min_child().node)->find_min_data_node(), upper);

      ++impl_.size_;
      return {node, true};
    }

    if (find_result.node_sub_cur < node->subfix_size_ && subfix_size > 0) {
      // split node, make parent node and hold this node
      node_base<value_type> *new_parent_node = node_new<node4<value_type>>();
      node_base<value_type> *new_child_node = node_new<node4<value_type>>();
      new_child_node->set_node_value(value);

      new_parent_node->set_node_subfix(node->subfix_start_,
                                       find_result.node_sub_cur);
      new_child_node->set_node_subfix(subfix + 1, subfix_size - 1);

      change_node_parent_child(new_parent_node, node,
                               find_result.parent_slot.node);

      char_type node_key_c = node->subfix_start_[find_result.node_sub_cur];

      new_parent_node->try_insert_child(node_key_c, node);
      new_parent_node->try_insert_child(subfix[0], new_child_node);

      node->truncate_node_prefix(find_result.node_sub_cur + 1);

      // search from parent node, find data node which key greater than target
      // or less tahn target.

      if (node_key_c > subfix[0]) {
        // the key of this node greater than target, find min data node from
        // this node
        insert_node_link(new_child_node, node->find_min_data_node(), upper);
      } else {
        // must not equal
        // the key of this node less than target, find max data node from this
        // node
        insert_node_link(new_child_node, node->find_max_data_node(), lower);
      }

      ++impl_.size_;
      return {new_child_node, true};
    }

    if (find_result.node_sub_cur == node->subfix_size_ && subfix_size > 0) {
      // append to this node child
      node_base<value_type> *new_node = node_new<node4<value_type>>();
      new_node->set_node_value(value);
      new_node->set_node_subfix(subfix + 1, subfix_size - 1);

    re_insert:
      const auto r1 = node->try_insert_child(subfix[0], new_node);
      if (r1.second) {
        child_slot<value_type> slot;

        // first find greater child in this node
        slot = node->find_greater_child(subfix[0]);
        if (slot.node != nullptr) {
          // find min data node
          insert_node_link(new_node, (*slot.node)->find_min_data_node(), upper);
          ++impl_.size_;
          return {new_node, true};
        }

        // second find less child in this node
        slot = node->find_less_child(subfix[0]);
        if (slot.node != nullptr) {
          // find max data node
          insert_node_link(new_node, (*slot.node)->find_max_data_node(), lower);
          ++impl_.size_;
          return {new_node, true};
        }

        // no other child, this node must has data
        insert_node_link(new_node, node, lower);
        ++impl_.size_;
        return {new_node, true};
      } else {
        // node is full, expand it
        node_base<value_type> *expanded_node = node_expand(node);
        change_node_parent_child(expanded_node, node,
                                 find_result.parent_slot.node);
        node_delete(node);
        node = expanded_node;
        // retry insert child
        goto re_insert;
      }
    }

    if (find_result.node_sub_cur < node->subfix_size_ && subfix_size == 0) {
      // split node, but parent is target node
      node_base<value_type> *new_parent_node = node_new<node4<value_type>>();
      new_parent_node->set_node_value(value);

      new_parent_node->set_node_subfix(node->subfix_start_,
                                       find_result.node_sub_cur);

      change_node_parent_child(new_parent_node, node,
                               find_result.parent_slot.node);

      new_parent_node->try_insert_child(
          node->subfix_start_[find_result.node_sub_cur], node);

      node->truncate_node_prefix(find_result.node_sub_cur + 1);

      // find the min data node
      insert_node_link(new_parent_node, node->find_min_data_node(), upper);

      ++impl_.size_;
      return {new_parent_node, true};
    }

    throw "bad judge";
  }

  std::pair<const node_link_base *, bool>
  lower_bound(const key_type &_key) const {
    const std::size_t key_size = _key.size();
    const char_type *key = _key.c_str();

    if (impl_.root_ == nullptr) {
      return {nullptr, false};
    }

    const find_result_type<value_type> find_result =
        find_last_node(impl_.root_, key, key_size);
    node_base<value_type> *node = find_result.node;
    const char_type *subfix = key + find_result.key_cur;
    const std::size_t subfix_size = key_size - find_result.key_cur;

    if (find_result.node_sub_cur == node->subfix_size_ && subfix_size == 0) {
      // find this node
      if (node->storage_valid_) {
        return {node, true};
      }
      node_base<value_type> *upper_node =
          (*node->find_min_child().node)->find_min_data_node();
      return {upper_node, false};
    }

    if (find_result.node_sub_cur < node->subfix_size_ && subfix_size > 0) {
      char_type node_key_c = node->subfix_start_[find_result.node_sub_cur];
      if (node_key_c > subfix[0]) {
        node_base<value_type> *upper_node = node->find_min_data_node();
        return {upper_node, false};
      }
      node_base<value_type> *lower_node = node->find_max_data_node();
      return {lower_node->next_, false};
    }

    if (find_result.node_sub_cur == node->subfix_size_ && subfix_size > 0) {
      child_slot<value_type> slot;
      // first find greater child in this node
      slot = node->find_greater_child(subfix[0]);
      if (slot.node != nullptr) {
        node_base<value_type> *upper_node = (*slot.node)->find_min_data_node();
        return {upper_node, false};
      }

      // second find less child in this node
      slot = node->find_less_child(subfix[0]);
      if (slot.node != nullptr) {
        node_base<value_type> *lower_node = (*slot.node)->find_max_data_node();
        return {lower_node->next_, false};
      }

      // no other child, this node must has data, but this key is less
      return {node->next_, false};
    }

    if (find_result.node_sub_cur < node->subfix_size_ && subfix_size == 0) {
      node_base<value_type> *upper_node = node->find_min_data_node();
      return {upper_node, false};
    }

    throw "bad judge";
  }

  void erase(node_base<value_type> *node) {
    if (!node->storage_valid_) {
      throw "erase no data node";
    }

    --impl_.size_;
    node->unset_node_value();
    erase_node_link(node);

    if (node->children_size_ > 1) {
      return;
    }

    while (true) {
      if (is_root(node)) {
        if (node->children_size_ == 1) {
          erase_node_with_one_child(node, nullptr);
          return;
        }

        // node->children_size_ == 0, erase root node
        node_delete(node);
        impl_.root_ = nullptr;
        return;
      }

      node_base<value_type> *parent_node = node->parent_;
      child_slot<value_type> parent_slot =
          parent_node->find_child(node->parent_c_);

      if (*parent_slot.node != node) {
        throw "bad found child";
      }

      if (node->children_size_ == 1) {
        erase_node_with_one_child(node, parent_slot.node);
        return;
      }

      // node->children_size_ == 0
      if (parent_node->storage_valid_ || parent_node->children_size_ > 2) {
        // delete child of parent
        parent_node->erase_child(parent_slot.c);
        node_delete(node);
        return;
      }

      // parent_node->children_size_ == 2
      parent_node->erase_child(parent_slot.c);
      node_delete(node);
      // need replace parent to the other child of parent
      // point parent to node, that mean erase parent
      node = parent_node;
    }
  }

  void swap(art_tree &other) {
    std::swap(impl_.size_, other.impl_.size_);
    std::swap(impl_.root_, other.impl_.root_);
    std::swap(impl_.node_counter_, other.impl_.node_counter_);

    node_link_base *tmp_node;

    tmp_node = impl_.dummy_.prev_;
    impl_.dummy_.next_->prev_ = &other.impl_.dummy_;
    tmp_node->next_ = &other.impl_.dummy_;

    tmp_node = other.impl_.dummy_.prev_;
    other.impl_.dummy_.next_->prev_ = &impl_.dummy_;
    tmp_node->next_ = &impl_.dummy_;

    std::swap(impl_.dummy_.next_, other.impl_.dummy_.next_);
    std::swap(impl_.dummy_.prev_, other.impl_.dummy_.prev_);
  }

  allocator_type get_allocator() const {
    return impl_.node_allocator_traits::template node_alloca_helper<
        node_type_guard<value_type>>;
  }

  struct art_tree_impl : public node_allocator_traits {
    art_tree_impl(const allocator_type &alloc = allocator_type())
        : root_(nullptr), size_(0), node_counter_(0),
          node_allocator_traits(alloc) {
      dummy_.prev_ = &dummy_;
      dummy_.next_ = &dummy_;
    }

    ~art_tree_impl() {}

    std::size_t size_;
    node_base<value_type> *root_;
    node_link_base dummy_;

    std::size_t node_counter_;
  };

  art_tree_impl impl_;
};

template <typename K, typename T,
          typename Alloc = std::allocator<std::pair<const K, T>>>
struct art {
  using key_type = K;
  using mapped_type = T;
  using value_type = std::pair<const key_type, mapped_type>;
  using allocator_type = Alloc;

  struct iterator {
    value_type &operator*() {
      return static_cast<node_base<value_type> *>(l_)->get_value();
    }
    value_type *operator->() {
      return &static_cast<node_base<value_type> *>(l_)->get_value();
    }
    iterator &operator++() {
      l_ = l_->next_;
      return *this;
    }
    iterator operator++(int) {
      iterator tmp;
      l_ = l_->next_;
      return tmp;
    }
    iterator &operator--() {
      l_ = l_->prev_;
      return *this;
    }
    iterator operator--(int) {
      iterator tmp;
      l_ = l_->prev_;
      return tmp;
    }
    bool operator==(const iterator &other) const { return l_ == other.l_; }
    bool operator!=(const iterator &other) const { return l_ != other.l_; }

    node_link_base *l_;
  };

  struct const_iterator {
    const_iterator() = default;
    const_iterator(const iterator iter) : l_(iter.l_) {}

    const value_type &operator*() {
      return static_cast<const node_base<value_type> *>(l_)->get_value();
    }
    const value_type *operator->() {
      return &static_cast<const node_base<value_type> *>(l_)->get_value();
    }

    const_iterator &operator++() {
      l_ = l_->next_;
      return *this;
    }
    const_iterator operator++(int) {
      const_iterator tmp;
      l_ = l_->next_;
      return tmp;
    }
    const_iterator &operator--() {
      l_ = l_->prev_;
      return *this;
    }
    const_iterator operator--(int) {
      const_iterator tmp;
      l_ = l_->prev_;
      return tmp;
    }
    bool operator==(const const_iterator &other) const {
      return l_ == other.l_;
    }
    bool operator!=(const const_iterator &other) const {
      return l_ != other.l_;
    }

    const node_link_base *l_;
  };

  art(const allocator_type &alloc = allocator_type()) : t_(alloc) {}
  art(const art &other, const allocator_type &alloc = allocator_type())
      : t_(alloc) {
    insert(other.begin(), other.end());
  }
  art(art &&other, const allocator_type &alloc = allocator_type()) : t_(alloc) {
    swap(other);
  }
  art(std::initializer_list<value_type> init,
      const allocator_type &alloc = allocator_type())
      : t_(alloc) {
    insert(init.begin(), init.end());
  }
  template <typename InputIterator>
  art(InputIterator first, InputIterator last,
      const allocator_type &alloc = allocator_type())
      : t_(alloc) {
    insert(first, last);
  }
  ~art() { clear(); }
  art &operator=(const art &other) {
    clear();
    insert(other.begin(), other.end());
    return *this;
  }
  art &operator=(art &&other) {
    swap(other);
    return *this;
  }
  art &operator=(std::initializer_list<value_type> ilist) {
    clear();
    insert(ilist.begin(), ilist.end());
    return *this;
  }

  bool empty() const { return t_.impl_.size_ == 0; }
  std::size_t size() const { return t_.impl_.size_; }

  iterator begin() {
    iterator iter;
    iter.l_ = t_.impl_.dummy_.next_;
    return iter;
  }
  iterator end() {
    iterator iter;
    iter.l_ = &t_.impl_.dummy_;
    return iter;
  }
  const_iterator begin() const {
    const_iterator iter;
    iter.l_ = t_.impl_.dummy_.next_;
    return iter;
  }
  const_iterator end() const {
    const_iterator iter;
    iter.l_ = static_cast<const node_link_base *>(&t_.impl_.dummy_);
    return iter;
  }

  mapped_type &at(const key_type &key) {
    iterator iter = find(key);
    if (iter != end()) {
      return iter->second;
    }
    throw "out of range";
  }
  const mapped_type &at(const key_type &key) const {
    const_iterator iter = find(key);
    if (iter != end()) {
      return iter->second;
    }
    throw "out of range";
  }
  mapped_type &operator[](const key_type &key) {
    auto r = insert(value_type(key, mapped_type()));
    return r.first->second;
  }

  void clear() {
    iterator iter = begin();
    while (iter != end()) {
      iterator tmp = iter;
      ++iter;
      erase(tmp);
    }

    if (t_.impl_.node_counter_ != 0) {
      throw "bad clear";
    }
  }
  std::pair<iterator, bool> insert(const value_type &value) {
    auto r = t_.insert(value);
    iterator iter;
    iter.l_ = r.first;
    return {iter, r.second};
  }
  // template <typename P> std::pair<iterator, bool> insert(P &&value) {
  //   throw("not implement");
  // }
  template <typename InputIt> void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }
  void insert(std::initializer_list<value_type> ilist) {
    insert(ilist.begin(), ilist.end());
  }
  // template <typename... Args>
  // std::pair<iterator, bool> emplace(Args &&...args) {
  //   throw("not implement");
  // }
  iterator erase(iterator pos) {
    iterator next = pos;
    ++next;
    t_.erase(static_cast<node_base<value_type> *>(pos.l_));
    return next;
  }
  iterator erase(const_iterator pos) {
    iterator next;
    next.l_ = const_cast<node_link_base *>(pos.l_);
    ++next;
    t_.erase(static_cast<node_base<value_type> *>(
        const_cast<node_link_base *>(pos.l_)));
    return next;
  }
  iterator erase(iterator first, iterator last) {
    while (first != last) {
      first = erase(first);
    }
    return last;
  }
  iterator erase(const_iterator first, const_iterator last) {
    iterator iter;
    while (first != last) {
      first = erase(first);
    }
    iter.l_ = const_cast<node_link_base *>(last.l_);
    return iter;
  }
  std::size_t erase(const key_type &key) {
    iterator iter = find(key);
    if (iter != end()) {
      erase(iter);
      return 1;
    }
    return 0;
  }
  void swap(art &other) { t_.swap(other.t_); }

  std::size_t count(const key_type &key) const {
    const_iterator iter = find(key);
    if (iter != end()) {
      return 1;
    }
    return 0;
  }
  iterator find(const key_type &key) {
    art::const_iterator citer = const_cast<const art &>(*this).find(key);
    iterator iter;
    iter.l_ = const_cast<node_link_base *>(citer.l_);
    return iter;
  }
  const_iterator find(const key_type &key) const {
    auto r = t_.find(key);
    if (r.second) {
      const_iterator iter;
      iter.l_ = r.first;
      return iter;
    }
    return end();
  }
  std::pair<iterator, iterator> equal_range(const key_type &key) {
    auto r = const_cast<const art &>(*this).equal_range(key);
    iterator i1, i2;
    i1.l_ = const_cast<node_link_base *>(r.first.l_);
    i2.l_ = const_cast<node_link_base *>(r.second.l_);
    return {i1, i2};
  }
  std::pair<const_iterator, const_iterator>
  equal_range(const key_type &key) const {
    const_iterator iter = find(key);
    if (iter != end()) {
      const_iterator tmp = iter;
      ++iter;
      return {tmp, iter};
    }
    return {end(), end()};
  }
  iterator lower_bound(const key_type &key) {
    const_iterator citer = const_cast<const art &>(*this).lower_bound(key);
    iterator iter;
    iter.l_ = const_cast<node_link_base *>(citer.l_);
    return iter;
  }
  const_iterator lower_bound(const key_type &key) const {
    const node_link_base *node = t_.lower_bound(key).first;
    if (node == nullptr) {
      return end();
    }

    const_iterator iter;
    iter.l_ = node;
    return iter;
  }
  iterator upper_bound(const key_type &key) {
    const_iterator citer = const_cast<const art &>(*this).upper_bound(key);
    iterator iter;
    iter.l_ = const_cast<node_link_base *>(citer.l_);
    return iter;
  }
  const_iterator upper_bound(const key_type &key) const {
    auto r = t_.lower_bound(key);
    const node_link_base *node = r.first;
    if (node == nullptr) {
      return end();
    }

    const_iterator iter;
    iter.l_ = node;

    if (r.second) {
      return ++iter;
    }
    return iter;
  }

  allocator_type get_allocator() const { return t_.get_allocator(); }

  art_tree<key_type, value_type, allocator_type> t_;
};

/******************  node4  *******************/

template <typename V> inline std::size_t node4<V>::node_size() const {
  return sizeof(decltype(*this));
}

template <typename V>
inline const_child_slot<V> node4<V>::find_child_impl(char_type c) const {
  const_child_slot<V> slot;
  for (uint8_t i = 0; i < node4<V>::children_size_; ++i) {
    if (keys_[i] == c) {
      slot.c = c;
      slot.node = &children_[i];
      return slot;
    }
  }
  slot.node = nullptr;
  return slot;
}

template <typename V>
inline child_slot<V> node4<V>::find_leq_child(char_type c) {
  child_slot<V> slot;
  slot.c = char_type_minium;
  slot.node = nullptr;
  for (uint8_t i = 0; i < node4<V>::children_size_; ++i) {
    if (keys_[i] <= c && slot.c <= keys_[i]) {
      slot.c = keys_[i];
      slot.node = &children_[i];
    }
  }
  return slot;
}

template <typename V>
inline child_slot<V> node4<V>::find_geq_child(char_type c) {
  child_slot<V> slot;
  slot.c = char_type_maxium;
  slot.node = nullptr;
  for (uint8_t i = 0; i < node4<V>::children_size_; ++i) {
    if (keys_[i] >= c && slot.c >= keys_[i]) {
      slot.c = keys_[i];
      slot.node = &children_[i];
    }
  }
  return slot;
}

template <typename V>
inline int node4<V>::get_all_children_impl(child_slot<V> slots[256]) {
  for (uint8_t i = 0; i < node4<V>::children_size_; ++i) {
    slots[i].c = keys_[i];
    slots[i].node = &children_[i];
  }
  return node4<V>::children_size_;
}

template <typename V>
inline std::pair<child_slot<V>, bool>
node4<V>::try_insert_child_impl(char_type c, node_base<V> *node) {
  child_slot<V> slot;
  if (node4<V>::children_size_ >= max_children_size) {
    return {slot, false};
  }

  for (uint8_t i = 0; i < node4<V>::children_size_; ++i) {
    if (keys_[i] == c) {
      throw "re-insert child";
    }
  }

  keys_[node4<V>::children_size_] = c;
  children_[node4<V>::children_size_] = node;
  ++node4<V>::children_size_;
  return {slot, true};
}

template <typename V> inline void node4<V>::erase_child(char_type c) {
  for (uint8_t i = 0; i < node4<V>::children_size_; ++i) {
    if (keys_[i] == c && children_[i] != nullptr) {
      keys_[i] = keys_[node4<V>::children_size_ - 1];
      children_[i] = children_[node4<V>::children_size_ - 1];
      --node4<V>::children_size_;
      return;
    }
  }
  throw "erase no found";
}

/******************  node4 end *******************/

/******************  node16  *******************/

template <typename V> inline std::size_t node16<V>::node_size() const {
  return sizeof(decltype(*this));
}

template <typename V>
inline const_child_slot<V> node16<V>::find_child_impl(char_type c) const {
  const_child_slot<V> slot;
  for (uint8_t i = 0; i < node16<V>::children_size_; ++i) {
    if (keys_[i] == c) {
      slot.c = c;
      slot.node = &children_[i];
      return slot;
    }
  }
  slot.node = nullptr;
  return slot;
}

template <typename V>
inline child_slot<V> node16<V>::find_leq_child(char_type c) {
  child_slot<V> slot;
  slot.c = char_type_minium;
  slot.node = nullptr;
  for (uint8_t i = 0; i < node16<V>::children_size_; ++i) {
    if (keys_[i] <= c && slot.c <= keys_[i]) {
      slot.c = keys_[i];
      slot.node = &children_[i];
    }
  }
  return slot;
}

template <typename V>
inline child_slot<V> node16<V>::find_geq_child(char_type c) {
  child_slot<V> slot;
  slot.c = char_type_maxium;
  slot.node = nullptr;
  for (uint8_t i = 0; i < node16<V>::children_size_; ++i) {
    if (keys_[i] >= c && slot.c >= keys_[i]) {
      slot.c = keys_[i];
      slot.node = &children_[i];
    }
  }
  return slot;
}

template <typename V>
inline int node16<V>::get_all_children_impl(child_slot<V> slots[256]) {
  for (uint8_t i = 0; i < node16<V>::children_size_; ++i) {
    slots[i].c = keys_[i];
    slots[i].node = &children_[i];
  }
  return node16<V>::children_size_;
}

template <typename V>
inline std::pair<child_slot<V>, bool>
node16<V>::try_insert_child_impl(char_type c, node_base<V> *node) {
  child_slot<V> slot;
  if (node16<V>::children_size_ >= max_children_size) {
    return {slot, false};
  }

  for (uint8_t i = 0; i < node16<V>::children_size_; ++i) {
    if (keys_[i] == c) {
      throw "re-insert child";
    }
  }

  keys_[node16<V>::children_size_] = c;
  children_[node16<V>::children_size_] = node;
  ++node16<V>::children_size_;
  return {slot, true};
}

template <typename V> inline void node16<V>::erase_child(char_type c) {
  for (uint8_t i = 0; i < node16<V>::children_size_; ++i) {
    if (keys_[i] == c && children_[i] != nullptr) {
      keys_[i] = keys_[node16<V>::children_size_ - 1];
      children_[i] = children_[node16<V>::children_size_ - 1];
      --node16<V>::children_size_;
      return;
    }
  }
  throw "erase no found";
}

/******************  node16 end *******************/

/******************  node48  *******************/

template <typename V> inline std::size_t node48<V>::node_size() const {
  return sizeof(decltype(*this));
}

template <typename V>
inline const_child_slot<V> node48<V>::find_child_impl(char_type c) const {
  const_child_slot<V> slot;

  if (children_[children_index()[c]] == nullptr) {
    slot.node = nullptr;
    return slot;
  }

  slot.c = c;
  slot.node = &children_[children_index()[c]];
  return slot;
}

template <typename V>
inline child_slot<V> node48<V>::find_leq_child(char_type c) {
  child_slot<V> slot;
  if (node48<V>::children_empty()) {
    slot.node = nullptr;
    return slot;
  }

  for (int i = c; i >= char_type_minium; i--) {
    if (children_[children_index()[i]] != nullptr) {
      slot.c = i;
      slot.node = &children_[children_index()[i]];
      return slot;
    }
  }

  slot.node = nullptr;
  return slot;
}

template <typename V>
inline child_slot<V> node48<V>::find_geq_child(char_type c) {
  child_slot<V> slot;
  if (node48<V>::children_empty()) {
    slot.node = nullptr;
    return slot;
  }

  for (int i = c; i <= char_type_maxium; ++i) {
    if (children_[children_index()[i]] != nullptr) {
      slot.c = i;
      slot.node = &children_[children_index()[i]];
      return slot;
    }
  }

  slot.node = nullptr;
  return slot;
}

template <typename V>
inline int node48<V>::get_all_children_impl(child_slot<V> slots[256]) {
  if (node48<V>::children_empty()) {
    return 0;
  }

  int w = 0;
  for (int i = char_type_minium; i <= char_type_maxium; ++i) {
    if (children_[children_index()[i]] != nullptr) {
      slots[w].c = static_cast<char>(i);
      slots[w].node = &children_[children_index()[i]];
      ++w;
    }
  }

  if (w != node48<V>::children_size_) {
    throw "get all failed";
  }

  return node48<V>::children_size_;
}

template <typename V>
inline std::pair<child_slot<V>, bool>
node48<V>::try_insert_child_impl(char_type c, node_base<V> *node) {
  child_slot<V> slot;
  if (node48<V>::children_size_ >= max_children_size) {
    return {slot, false};
  }

  if (children_[children_index()[c]] != nullptr) {
    throw "re-insert child";
  }

  children_index()[c] = node48<V>::children_size_ + 1;
  children_[children_index()[c]] = node;
  ++node48<V>::children_size_;
  return {slot, true};
}

template <typename V> inline void node48<V>::erase_child(char_type c) {
  if (children_[children_index()[c]] == nullptr) {
    throw "erase no found";
  }

  char_type moved_index = children_index()[c];
  node_base<V> *moved_node = children_[node48<V>::children_size_];
  children_[children_index()[c]] = moved_node;
  children_index()[c] = 0;

  // if erase last children, dont need to search
  if (moved_index != node48<V>::children_size_) {
    for (int i = char_type_minium; i <= char_type_maxium; ++i) {
      if (children_[children_index()[i]] == moved_node) {
        children_index()[i] = moved_index;
        break;
      }
    }
  }

  --node48<V>::children_size_;
}

/******************  node48 end *******************/

/******************  node256  *******************/

template <typename V> inline std::size_t node256<V>::node_size() const {
  return sizeof(decltype(*this));
}

template <typename V>
inline const_child_slot<V> node256<V>::find_child_impl(char_type c) const {
  const_child_slot<V> slot;
  if (children()[c] == nullptr) {
    slot.node = nullptr;
    return slot;
  }

  slot.c = c;
  slot.node = &children()[c];
  return slot;
}

template <typename V>
inline child_slot<V> node256<V>::find_leq_child(char_type c) {
  child_slot<V> slot;
  if (node256<V>::children_empty()) {
    slot.node = nullptr;
    return slot;
  }

  for (int i = c; i > char_type_minium; i--) {
    if (children()[i] != nullptr) {
      slot.c = i;
      slot.node = &children()[i];
      return slot;
    }
  }

  slot.node = nullptr;
  return slot;
}

template <typename V>
inline child_slot<V> node256<V>::find_geq_child(char_type c) {
  child_slot<V> slot;
  if (node256<V>::children_empty()) {
    slot.node = nullptr;
    return slot;
  }

  for (int i = c; i < char_type_maxium; ++i) {
    if (children()[i] != nullptr) {
      slot.c = i;
      slot.node = &children()[i];
      return slot;
    }
  }

  slot.node = nullptr;
  return slot;
}

template <typename V>
inline int node256<V>::get_all_children_impl(child_slot<V> slots[256]) {
  if (node256<V>::children_empty()) {
    return 0;
  }

  int w = 0;
  for (int i = char_type_minium; i <= char_type_maxium; ++i) {
    if (children()[i] != nullptr) {
      slots[w].c = static_cast<char>(i);
      slots[w].node = &children()[i];
      ++w;
    }
  }

  if (w != node256<V>::children_size_) {
    throw "get all failed";
  }

  return node256<V>::children_size_;
}

template <typename V>
inline std::pair<child_slot<V>, bool>
node256<V>::try_insert_child_impl(char_type c, node_base<V> *node) {
  if (node256<V>::children_size_ >= max_children_size) {
    throw "node256 overflow";
  }

  if (children()[c] != nullptr) {
    throw "re-insert child";
  }

  child_slot<V> slot;
  children()[c] = node;
  ++node256<V>::children_size_;
  return {slot, true};
}

template <typename V> inline void node256<V>::erase_child(char_type c) {
  if (children()[c] == nullptr) {
    throw "erase no found";
  }

  children()[c] = nullptr;
  --node256<V>::children_size_;
}

/******************  node256 end *******************/

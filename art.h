#ifndef __ART_TREE_H__
#define __ART_TREE_H__

#include <cassert>
#include <cstring>
#include <functional>

template <typename value_type, bool constant_value> struct _art_impl {
  enum node_type { NODE4, NODE16, NODE48, NODE256 };

  struct node4;
  struct node16;
  struct node48;
  struct node256;
  struct node_impl;

  template <typename NODE, typename PLACEHOLD = void>
  struct node_level_up_helper;
  template <typename PLACEHOLD> struct node_level_up_helper<node4, PLACEHOLD> {
    using type = node48;
  };
  // template <typename PLACEHOLD>
  // struct node_level_up_helper<node16, PLACEHOLD> {
  //   using type = node48;
  // };
  template <typename PLACEHOLD> struct node_level_up_helper<node48, PLACEHOLD> {
    using type = node256;
  };
  template <typename PLACEHOLD>
  struct node_level_up_helper<node256, PLACEHOLD> {
    using type = node256;
  };

  static constexpr size_t value_size = sizeof(value_type);

  enum find_in_subkey_state {
    ok,
    fail_in_midway,
    fail_in_tail,
  };

  struct find_result_t {
    bool is_find;
    char last_c;
    node_impl *last_find_node;
    size_t node_subkey_divide_i;
    size_t key_divide_i;
    node_impl **parent_pptr;
  };

  struct val_ptr_wrapper {
    value_type *val_ptr[constant_value ? 1 : 0];
    val_ptr_wrapper() {
      if (constant_value) {
        val_ptr[0] = nullptr;
      }
    }
  };

  struct node_header {
    // uint8_t lock_bit : 1;
    bool is_key : 1;
    bool val_stick : 1;
    bool val_init : 1;
    enum node_type type : 2;
    uint32_t child_cnt : 9;
    size_t node_capacity;
    size_t subkey_len;
    const char *subkey;
    node_impl *parent_ptr;
    val_ptr_wrapper val_ptr;

    node_header(node_type type, size_t cap)
        : /* lock_bit(0), */ is_key(false), val_stick(false), val_init(false),
          type(type), child_cnt(0), node_capacity(cap), subkey_len(0),
          parent_ptr(nullptr) {}
  };

  struct node_impl {
    node_header header;
    node_impl(node_type type, size_t cap) : header(type, cap) {}

    value_type *val_ptr() {
      if (value_size > 8) {
        return header.val_ptr.val_ptr[0];
      } else {
        return (value_type *)header.val_ptr.val_ptr;
      }
    }

    void set_val_ptr(value_type *ptr) {
      if (value_size > 8) {
        header.val_ptr.val_ptr[0] = ptr;
      }
    }

    template <typename... Args> void val_ctor(Args &&...args) {
      if (constant_value) {
        if (header.val_init) {
          *val_ptr() = value_type(std::move(args)...);
        } else if (!header.val_stick && val_ptr() == nullptr) {
          set_val_ptr(new value_type(std::move(args)...));
          header.val_init = true;
        } else {
          new (val_ptr()) value_type(std::move(args)...);
          header.val_init = true;
        }
      }
    }

    void val_dtor() {
      if (constant_value && header.val_init) {
        if (header.val_stick) {
          val_ptr()->~value_type();
        } else if (value_size > 8) {
          _m_delete_value(val_ptr());
          set_val_ptr(nullptr);
        }
        header.val_init = false;
      }
    }

    find_in_subkey_state search_in_subkey(const char *key, size_t len,
                                          size_t &subkey_i, size_t &key_i) {
      size_t min_s = std::min(len - key_i, header.subkey_len);
      const char *key_tmp = key + key_i;
      size_t subkey_i_tmp = 0;

      while (1) {
        if (subkey_i_tmp + 7 >= min_s) {
          for (; subkey_i_tmp < min_s &&
                 header.subkey[subkey_i_tmp] == key_tmp[subkey_i_tmp];
               ++subkey_i_tmp) {
          }
          break;
        }
        if (*(uint64_t *)(header.subkey + subkey_i_tmp) !=
            *(uint64_t *)(key_tmp + subkey_i_tmp)) {
          for (; header.subkey[subkey_i_tmp] == key_tmp[subkey_i_tmp];
               ++subkey_i_tmp) {
          }
          break;
        }
        subkey_i_tmp += 8;
      }

      subkey_i = subkey_i_tmp;
      key_i += subkey_i_tmp;

      if (subkey_i_tmp == header.subkey_len) {
        if (key_i != len) {
          return fail_in_tail;
        }
        if (header.is_key) {
          return ok;
        }
      }
      return fail_in_midway;
    }

    template <typename... Args>
    node_impl *
    split_subkey_and_insert(const char *key_divide, size_t node_subkey_divide,
                            size_t key_divide_len, node_impl **parent_pptr,
                            Args &&...args) {
      char c = header.subkey[node_subkey_divide];
      node4 *parent_node;
      node_impl *ret;
      if (constant_value && key_divide_len == 0) {
        parent_node =
            _m_new_node_with_val_mem<node4>(header.subkey, node_subkey_divide);
      } else {
        parent_node = _m_create_node<node4>(header.subkey, node_subkey_divide);
      }
      header.subkey_len -= node_subkey_divide + 1;
      header.subkey += node_subkey_divide + 1;

      parent_node->header.parent_ptr = header.parent_ptr;
      parent_node->insert_child(this, c);

      if (key_divide_len > 0) {
        node_impl *sibling_p = parent_node->try_insert_child(
            *key_divide, key_divide + 1, key_divide_len - 1);

        sibling_p->val_ctor(std::move(args)...);
        sibling_p->header.is_key = true;
        ret = sibling_p;
      } else {
        parent_node->val_ctor(std::move(args)...);
        parent_node->header.is_key = true;
        ret = parent_node;
      }

      *parent_pptr = parent_node;
      return ret;
    }

    template <typename... Args>
    node_impl *node_insert_child_with_subkey(const char *key_divide,
                                             size_t node_subkey_divide,
                                             size_t key_divide_len,
                                             node_impl **parent_pptr,
                                             Args &&...args) {
      node_impl *child_p;
      if (node_subkey_divide == header.subkey_len) {
        child_p =
            try_insert_child(*key_divide, key_divide + 1, key_divide_len - 1);
        if (child_p != nullptr) {
          child_p->val_ctor(std::move(args)...);
          child_p->header.is_key = true;
          return child_p;
        }

        node_impl *new_node = level_up();

        if (new_node == this) {
          child_p = new_node->try_insert_child(*key_divide, key_divide + 1,
                                               key_divide_len - 1);
        } else if (header.node_capacity >= sizeof(node4) + key_divide_len - 1) {
          bool val_stick = header.val_stick;
          value_type *ptr = val_ptr();
          node4 *child_node = new (this) node4(header.node_capacity);
          if (constant_value) {
            child_node->header.val_stick = val_stick;
            set_val_ptr(ptr);
          }
          child_node->append_key_ahead(key_divide + 1, key_divide_len - 1);
          new_node->insert_child(child_node, *key_divide);
          child_p = child_node;
        } else {
          child_p = new_node->try_insert_child(*key_divide, key_divide + 1,
                                               key_divide_len - 1);
          _m_delete_node(this);
        }

        child_p->val_ctor(std::move(args)...);
        child_p->header.is_key = true;
        *parent_pptr = new_node;
      } else {
        child_p = split_subkey_and_insert(key_divide, node_subkey_divide,
                                          key_divide_len, parent_pptr,
                                          std::move(args)...);
      }
      return child_p;
    }

    void clear_loop() {
      if (header.child_cnt > 0) {
        children_foreach([](node_impl *child_p, char c, int i) {
          if (child_p) {
            child_p->clear_loop();
          }
        });
      }
      _m_delete_node(this);
    }

    void may_erase_self(char last_c, node_impl **parent_pptr) {
      if (header.child_cnt == 0) {
        if (header.parent_ptr != nullptr) {
          header.parent_ptr->remove_child(this, last_c);
        } else {
          *parent_pptr = nullptr;
        }
        _m_delete_node(this);
      } else if (header.child_cnt == 1) {
        children_any([parent_pptr, this](node_impl *child_p, char c, int i) {
          if (child_p->append_key_ahead(header.subkey - 1,
                                        header.subkey_len + 1)) {
            ((char *)child_p->header.subkey)[0] = c;
            child_p->header.parent_ptr = header.parent_ptr;
            *parent_pptr = child_p;
            _m_delete_node(this);
          }
          return true;
        });
      }
    }

    virtual void
    children_foreach(std::function<void(node_impl *, char, int)> &&fn) = 0;
    virtual void
    children_any(std::function<bool(node_impl *, char, int)> &&fn) = 0;
    virtual bool inner_find_child(char c, node_impl *&p, node_impl **&pp) = 0;
    virtual void insert_child(node_impl *child_p, char c) = 0;
    virtual node_impl *try_insert_child(char c, const char *child_subkey,
                                        size_t child_subkey_len) = 0;
    virtual void remove_child(node_impl *child_p, char c) = 0;
    virtual void clear_children() = 0;
    virtual bool append_key_ahead(const char *subkey, size_t subkey_len) = 0;
    virtual node_impl *level_up() = 0;
  };

  template <typename NODE>
  static NODE *_m_create_node(const char *subkey, size_t subkey_len) {
    size_t cap = (sizeof(NODE) + subkey_len + 63) / 64 * 64;
    size_t data_cap = cap - sizeof(NODE);

    NODE *node = (NODE *)::operator new(cap);
    new (node) NODE(cap);

    node->append_key_ahead(subkey, subkey_len);
    return node;
  }

  template <typename NODE>
  static NODE *_m_new_node_with_val_mem(const char *subkey, size_t subkey_len) {
    size_t cap = (sizeof(NODE) + ((value_size > 8) ? sizeof(value_type) : 0) +
                  subkey_len + 63) /
                 64 * 64;
    NODE *node = (NODE *)::operator new(cap);

    cap -= ((value_size > 8) ? sizeof(value_type) : 0);
    size_t data_cap = cap - sizeof(NODE);

    new (node) NODE(cap);

    node->header.val_stick = true;
    node->set_val_ptr((value_type *)((char *)node + cap));

    node->append_key_ahead(subkey, subkey_len);
    return node;
  }

  static void _m_delete_node(node_impl *p) {
    if (constant_value && p->header.is_key) {
      p->val_dtor();
    }

    ::operator delete(p);
  }

  template <typename... Args> static value_type *_m_new_value(Args &&...args) {
    return new value_type(std::move(args)...);
  }

  static void _m_delete_value(value_type *p) { delete p; }

  struct node4 : public node_impl {
    char kc[4];
    node_impl *child_ptr[4];
    char data[0];

    node4(size_t cap) : node_impl(NODE4, cap) {
      size_t data_cap = cap - sizeof(node4);
      this->header.subkey = data + data_cap;
      clear_children();
    }

    void children_foreach(
        std::function<void(node_impl *, char, int)> &&fn) override {
      for (int i = 0; i < 4; ++i) {
        if (child_ptr[i] != nullptr) {
          fn(child_ptr[i], kc[i], i);
        }
      }
    }

    void
    children_any(std::function<bool(node_impl *, char, int)> &&fn) override {
      for (int i = 0; i < 4; ++i) {
        if (child_ptr[i] != nullptr && fn(child_ptr[i], kc[i], i)) {
          break;
        }
      }
    }

    bool inner_find_child(char c, node_impl *&p, node_impl **&pp) override {
      for (int i = 0; i < 4; ++i) {
        if (kc[i] == c) {
          if (child_ptr[i] == nullptr) {
            return false;
          }
          pp = &child_ptr[i];
          p = child_ptr[i];
          return true;
        }
      }
      return false;
    }

    void insert_child(node_impl *child_p, char c, int i) {
      kc[i] = c;
      child_p->header.parent_ptr = this;
      child_ptr[i] = child_p;
      ++this->header.child_cnt;
    }

    void insert_child(node_impl *child_p, char c) override {
      for (int i = 0; i < 4; ++i) {
        if (child_ptr[i] == nullptr) {
          insert_child(child_p, c, i);
          return;
        }
      }
    }

    node_impl *try_insert_child(char c, const char *child_subkey,
                                size_t child_subkey_len) override {
      for (int i = 0; i < 4; ++i) {
        if (child_ptr[i] == nullptr) {
          node4 *child_node =
              constant_value
                  ? _m_new_node_with_val_mem<node4>(child_subkey,
                                                    child_subkey_len)
                  : _m_create_node<node4>(child_subkey, child_subkey_len);
          insert_child(child_node, c, i);
          return child_node;
        }
      }
      return nullptr;
    }

    void remove_child(node_impl *child_p, char c) override {
      for (int i = 0; i < 4; ++i) {
        if (kc[i] == c) {
          child_ptr[i] = nullptr;
          --this->header.child_cnt;
          break;
        }
      }
    }

    void clear_children() override {
      this->header.child_cnt = 0;
      memset(child_ptr, 0, sizeof(child_ptr));
    }

    bool append_key_ahead(const char *subkey, size_t subkey_len) override {
      if (this->header.subkey - subkey_len >= data) {
        this->header.subkey -= subkey_len;
        memcpy((char *)this->header.subkey, subkey, subkey_len);
        this->header.subkey_len += subkey_len;
        return true;
      }
      return false;
    }

    node_impl *level_up() override {
      node48 *new_node;
      if (this->header.node_capacity <
          sizeof(node48) + this->header.subkey_len) {
        if (constant_value && this->header.is_key) {
          new_node = _m_new_node_with_val_mem<node48>(this->header.subkey,
                                                      this->header.subkey_len);
          new_node->val_ctor(*this->val_ptr());
        } else {
          new_node = _m_create_node<node48>(this->header.subkey,
                                            this->header.subkey_len);
        }
        new_node->header.is_key = this->header.is_key;
        for (int i = 0; i < 4; ++i) {
          new_node->insert_child(child_ptr[i], kc[i], i);
        }
        new_node->header.parent_ptr = this->header.parent_ptr;
      } else {
        char kc_tmp[4];
        node_impl *child_ptr_tmp[4];
        memcpy(kc_tmp, kc, sizeof(kc));
        memcpy(child_ptr_tmp, child_ptr, sizeof(child_ptr));

        node_header tmp = this->header;
        new_node = new (this) node48(tmp.node_capacity);
        new_node->header = tmp;
        new_node->header.type = NODE48;

        for (int i = 0; i < 4; ++i) {
          new_node->insert_child(child_ptr_tmp[i], kc_tmp[i], i);
        }

        assert(new_node->header.subkey >= new_node->data);
      }

      return new_node;
    }
  };

  struct node16 : public node_impl {
    char kc[16];
    node_impl *child_ptr[16];
    char data[0];

    node16(size_t cap) : node_impl(NODE16, cap) {}
  };

  struct node48 : public node_impl {
    static const uint8_t invalid_kc = 255;

    uint64_t node48_bitset : 48;
    uint8_t kc[256];
    node_impl *child_ptr[48];
    char data[0];

    node48(size_t cap) : node_impl(NODE48, cap) {
      size_t data_cap = cap - sizeof(node48);
      this->header.subkey = data + data_cap;
      clear_children();
    }

    void children_foreach(
        std::function<void(node_impl *, char, int)> &&fn) override {
      for (int i = 0; i < 256; ++i) {
        if (kc[i] != invalid_kc) {
          fn(child_ptr[kc[i]], (char)i, i);
        }
      }
    }

    void
    children_any(std::function<bool(node_impl *, char, int)> &&fn) override {
      for (int i = 0; i < 256; ++i) {
        if (kc[i] != invalid_kc && fn(child_ptr[kc[i]], (char)i, i)) {
          break;
        }
      }
    }

    bool inner_find_child(char c, node_impl *&p, node_impl **&pp) override {
      uint8_t i = kc[c];
      if (i != invalid_kc) {
        pp = &child_ptr[i];
        p = child_ptr[i];
        return true;
      }
      return false;
    }

    void insert_child(node_impl *child_p, char c, int free_slot) {
      kc[c] = free_slot;
      node48_bitset |= 1ul << free_slot;
      child_ptr[free_slot] = child_p;
      child_p->header.parent_ptr = this;
      ++this->header.child_cnt;
    }

    void insert_child(node_impl *child_p, char c) override {
      int free_slot = ffsll(~((uint64_t)node48_bitset)) - 1;
      insert_child(child_p, c, free_slot);
    }

    node_impl *try_insert_child(char c, const char *child_subkey,
                                size_t child_subkey_len) override {
      int free_slot = ffsll(~((uint64_t)node48_bitset)) - 1;

      if (free_slot == 48) {
        return nullptr;
      }

      node4 *child_node =
          constant_value
              ? _m_new_node_with_val_mem<node4>(child_subkey, child_subkey_len)
              : _m_create_node<node4>(child_subkey, child_subkey_len);
      insert_child(child_node, c, free_slot);

      return child_node;
    }

    void remove_child(node_impl *child_p, char c) override {
      uint8_t i = kc[c];
      child_ptr[i] = nullptr;
      node48_bitset &= ~(1ul << i);
      --this->header.child_cnt;
    }

    void clear_children() override {
      node48_bitset = 0;
      memset(kc, invalid_kc, sizeof(kc));
      memset(child_ptr, 0, sizeof(child_ptr));
    }

    bool append_key_ahead(const char *subkey, size_t subkey_len) override {
      if (this->header.subkey - subkey_len >= data) {
        this->header.subkey -= subkey_len;
        memcpy((char *)this->header.subkey, subkey, subkey_len);
        this->header.subkey_len += subkey_len;
        return true;
      }
      return false;
    }

    node_impl *level_up() override {
      node256 *new_node;
      if (this->header.node_capacity <
          sizeof(node256) + this->header.subkey_len) {
        if (constant_value && this->header.is_key) {
          new_node = _m_new_node_with_val_mem<node256>(this->header.subkey,
                                                       this->header.subkey_len);
          new_node->val_ctor(*this->val_ptr());
        } else {
          new_node = _m_create_node<node256>(this->header.subkey,
                                             this->header.subkey_len);
        }
        new_node->header.is_key = this->header.is_key;
        for (int i = 0; i < 256; ++i) {
          if (kc[i] != invalid_kc) {
            new_node->insert_child(child_ptr[kc[i]], i);
          }
        }
        new_node->header.parent_ptr = this->header.parent_ptr;
      } else {
        uint8_t kc_tmp[256];
        node_impl *child_ptr_tmp[48];
        memcpy(kc_tmp, kc, sizeof(kc));
        memcpy(child_ptr_tmp, child_ptr, sizeof(child_ptr));

        node_header tmp = this->header;
        new_node = new (this) node256(tmp.node_capacity);
        new_node->header = tmp;
        new_node->header.type = NODE256;

        for (int i = 0; i < 256; ++i) {
          if (kc_tmp[i] != invalid_kc) {
            new_node->insert_child(child_ptr_tmp[kc_tmp[i]], i);
          }
        }

        assert(new_node->header.subkey >= new_node->data);
      }
      return new_node;
    }
  };

  struct node256 : public node_impl {
    node_impl *child_ptr[256];
    char data[0];

    node256(size_t cap) : node_impl(NODE256, cap) {
      size_t data_cap = cap - sizeof(node256);
      this->header.subkey = data + data_cap;
      clear_children();
    }

    void children_foreach(
        std::function<void(node_impl *, char, int)> &&fn) override {
      for (int i = 0; i < 256; ++i) {
        if (child_ptr[i] != nullptr) {
          fn(child_ptr[i], (char)i, i);
        }
      }
    }

    void
    children_any(std::function<bool(node_impl *, char, int)> &&fn) override {
      for (int i = 0; i < 256; ++i) {
        if (child_ptr[i] != nullptr && fn(child_ptr[i], (char)i, i)) {
          break;
        }
      }
    }

    bool inner_find_child(char c, node_impl *&p, node_impl **&pp) override {
      if (child_ptr[c] != nullptr) {
        pp = &child_ptr[c];
        p = child_ptr[c];
        return true;
      }
      return false;
    }

    void insert_child(node_impl *child_p, char c) override {
      child_p->header.parent_ptr = this;
      child_ptr[c] = child_p;
      ++this->header.child_cnt;
    }

    node_impl *try_insert_child(char c, const char *child_subkey,
                                size_t child_subkey_len) override {
      if (child_ptr[c] == nullptr) {
        node4 *child_node =
            constant_value
                ? _m_new_node_with_val_mem<node4>(child_subkey,
                                                  child_subkey_len)
                : _m_create_node<node4>(child_subkey, child_subkey_len);
        insert_child(child_node, c);
        return child_node;
      }
      return nullptr;
    }

    void remove_child(node_impl *child_p, char c) override {
      child_ptr[c] = nullptr;
      --this->header.child_cnt;
    }

    void clear_children() override {
      this->header.child_cnt = 0;
      memset(child_ptr, 0, sizeof(child_ptr));
    }

    bool append_key_ahead(const char *subkey, size_t subkey_len) override {
      if (this->header.subkey - subkey_len >= data) {
        this->header.subkey -= subkey_len;
        memcpy((char *)this->header.subkey, subkey, subkey_len);
        this->header.subkey_len += subkey_len;
        return true;
      }
      return false;
    }

    node_impl *level_up() override { return nullptr; }
  };

  uint8_t _m_lock_bit : 1;
  size_t _m_size;
  node_impl *_m_root;

  _art_impl() : _m_lock_bit(0), _m_root(nullptr), _m_size(0) {}
  ~_art_impl() { clear(); }
  _art_impl(_art_impl &s) { swap(s); }
  _art_impl(_art_impl &&s) { swap(std::move(s)); }
  _art_impl &operator=(_art_impl &s) {
    swap(s);
    return *this;
  }
  _art_impl &operator=(_art_impl &&s) {
    swap(std::move(s));
    return *this;
  }

  size_t size() const { return _m_size; }
  bool empty() const { return size() == 0; }

  void clear() {
    if (empty()) {
      return;
    }
    _m_root->clear_loop();
    _m_size = 0;
    _m_root = nullptr;
  }

  void swap(_art_impl &s) {
    std::swap(_m_size, s._m_size);
    std::swap(_m_root, s._m_root);
  }

  void swap(_art_impl &&s) {
    std::swap(_m_size, s._m_size);
    std::swap(_m_root, s._m_root);
  }

  template <typename... Args>
  std::pair<bool, value_type *> _m_insert_impl(const char *key, size_t len,
                                               Args &&...args) {
    find_result_t res = _m_inner_find(key, len);
    if (res.is_find) {
      res.last_find_node->val_ctor(std::move(args)...);
      return {false, res.last_find_node->val_ptr()};
    }

    ++_m_size;

    if (res.last_find_node != nullptr) {
      if (res.key_divide_i == len &&
          res.node_subkey_divide_i == res.last_find_node->header.subkey_len) {
        res.last_find_node->val_ctor(std::move(args)...);
        res.last_find_node->header.is_key = true;
        return {true, res.last_find_node->val_ptr()};
      } else {
        node_impl *node = res.last_find_node->node_insert_child_with_subkey(
            key + res.key_divide_i, res.node_subkey_divide_i,
            len - res.key_divide_i, res.parent_pptr, std::move(args)...);
        return {true, node->val_ptr()};
      }
    } else {
      node4 *node;
      if (constant_value) {
        node = _m_new_node_with_val_mem<node4>(key, len);
        node->val_ctor(std::move(args)...);
      } else {
        node = _m_create_node<node4>(key, len);
      }
      node->header.is_key = true;
      *res.parent_pptr = node;
      return {true, node->val_ptr()};
    }
  }

  bool erase(const char *key, size_t len) {
    find_result_t res = _m_inner_find(key, len);
    if (!res.is_find) {
      return false;
    }

    --_m_size;
    res.last_find_node->header.is_key = false;
    res.last_find_node->val_dtor();
    res.last_find_node->may_erase_self(res.last_c, res.parent_pptr);
    return true;
  }

  find_result_t _m_inner_find(const char *key, size_t len) {
    find_result_t res = {false, 0, _m_root, -1ul, 0, &_m_root};
    node_impl *&p = res.last_find_node;
    node_impl **&pp = res.parent_pptr;
    size_t &key_i = res.key_divide_i;
    size_t &subkey_i = res.node_subkey_divide_i;
    bool ret;
    while (p != nullptr) {
      find_in_subkey_state state =
          p->search_in_subkey(key, len, subkey_i, key_i);
      if (state != fail_in_tail || !p->inner_find_child(key[key_i], p, pp)) {
        res.is_find = state == ok;
        break;
      }
      res.last_c = key[key_i];
      ++key_i;
    }
    return res;
  }

  static const char *_m_node_type_str(node_type type) {
    switch (type) {
    case NODE4:
      return "NODE4";
    case NODE16:
      return "NODE16";
    case NODE48:
      return "NODE48";
    case NODE256:
      return "NODE256";
    }
    return "";
  }

  static void _m_dump_loop(node_impl *p, int depth) {
    if (p == nullptr)
      return;

    switch (p->header.type) {
    case NODE4: {
      node4 *node = (node4 *)p;
      for (int i = 0; i < depth; ++i) {
        putchar('\t');
      }
      printf("[%p] type: %s, is key: %d, cap: %lu, subkey_len: %lu, parent: %p",
             p, _m_node_type_str(p->header.type), p->header.is_key,
             p->header.node_capacity, p->header.subkey_len,
             p->header.parent_ptr);
      if (p->header.subkey_len > 0) {
        printf(", subkey: ");
      }
      for (size_t i = 0; i < p->header.subkey_len; ++i) {
        putchar(node->header.subkey[i]);
      }
      putchar('\n');
      for (int i = 0; i < depth; ++i) {
        putchar('\t');
      }
      for (int i = 0; i < 4; ++i) {
        if (node->child_ptr[i] != nullptr) {
          printf("[%d, %c] --> %p, ", i, node->kc[i], node->child_ptr[i]);
        }
      }
      putchar('\n');
      for (int i = 0; i < 4; ++i) {
        if (node->child_ptr[i] != nullptr) {
          for (int i = 0; i < depth; ++i) {
            putchar('\t');
          }
          printf("[%c] --> \n", node->kc[i]);
          _m_dump_loop(node->child_ptr[i], depth + 1);
        }
      }
      putchar('\n');
    } break;
    case NODE16: {
      node16 *node = (node16 *)p;
      assert(false);
    } break;
    case NODE48: {
      node48 *node = (node48 *)p;
      for (int i = 0; i < depth; ++i) {
        putchar('\t');
      }
      printf("[%p] type: %s, is key: %d, cap: %lu, bitset: %08lx, subkey_len: "
             "%lu, parent: %p",
             p, _m_node_type_str(p->header.type), p->header.is_key,
             p->header.node_capacity, node->node48_bitset, p->header.subkey_len,
             p->header.parent_ptr);
      if (p->header.subkey_len > 0) {
        printf(", subkey: ");
      }
      for (size_t i = 0; i < p->header.subkey_len; ++i) {
        putchar(node->header.subkey[i]);
      }
      putchar('\n');
      for (int i = 0; i < depth; ++i) {
        putchar('\t');
      }
      for (int i = 0; i < 256; ++i) {
        if (node->kc[i] != node48::invalid_kc) {
          printf("[%c, %d] --> %p, ", (char)i, node->kc[i],
                 node->child_ptr[node->kc[i]]);
        }
      }
      putchar('\n');
      for (int i = 0; i < 256; ++i) {
        if (node->kc[i] != node48::invalid_kc) {
          for (int i = 0; i < depth; ++i) {
            putchar('\t');
          }
          printf("[%c, %d] --> \n", (char)i, node->kc[i]);
          _m_dump_loop(node->child_ptr[node->kc[i]], depth + 1);
        }
      }
      putchar('\n');
    } break;
    case NODE256: {
      node256 *node = (node256 *)p;
      for (int i = 0; i < depth; ++i) {
        putchar('\t');
      }
      printf("[%p] type: %s, is key: %d, cap: %lu, subkey_len: %lu, parent: %p",
             p, _m_node_type_str(p->header.type), p->header.is_key,
             p->header.node_capacity, p->header.subkey_len,
             p->header.parent_ptr);
      if (p->header.subkey_len > 0) {
        printf(", subkey: ");
      }
      for (size_t i = 0; i < p->header.subkey_len; ++i) {
        putchar(node->header.subkey[i]);
      }
      putchar('\n');
      for (int i = 0; i < depth; ++i) {
        putchar('\t');
      }
      for (int i = 0; i < 256; ++i) {
        if (node->child_ptr[i] != nullptr) {
          printf("[%d, %c] --> %p, ", i, (char)i, node->child_ptr[i]);
        }
      }
      putchar('\n');
      for (int i = 0; i < 256; ++i) {
        if (node->child_ptr[i] != nullptr) {
          for (int i = 0; i < depth; ++i) {
            putchar('\t');
          }
          printf("[%c] --> \n", (char)i);
          _m_dump_loop(node->child_ptr[i], depth + 1);
        }
      }
      putchar('\n');
    } break;
    }
  }

  void _m_dump() {
    printf("size: %lu\n", size());
    _m_dump_loop(_m_root, 0);
  }
};

struct art_set : public _art_impl<int, false> {
  bool insert(const char *key, size_t len) {
    return this->_m_insert_impl(key, len).first;
  }

  bool find(const char *key, size_t len) {
    return this->_m_inner_find(key, len).is_find;
  }
};

template <typename value_type>
struct art_map : public _art_impl<value_type, true> {
  std::pair<bool, value_type *> insert(const char *key, size_t len,
                                       const value_type &value) {
    return this->_m_insert_impl(key, len, value);
  }

  std::pair<bool, value_type *> insert(const char *key, size_t len,
                                       value_type &&value) {
    return this->_m_insert_impl(key, len, std::move(value));
  }

  template <typename... Args>
  std::pair<bool, value_type *> emplace(const char *key, size_t len,
                                        Args &&...args) {
    return this->_m_insert_impl(key, len, std::move(args)...);
  }

  std::pair<bool, value_type *> find(const char *key, size_t len) {
    auto res = this->_m_inner_find(key, len);
    if (res.is_find) {
      return {true, res.last_find_node->val_ptr()};
    } else {
      return {false, nullptr};
    }
  }
};

#endif // __ART_TREE_H__

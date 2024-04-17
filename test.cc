#include "art.h"
#include <algorithm>
#include <chrono>
#include <random>

using namespace std;

template <typename K, typename V>
void print_preorder(art_tree<K, V, std::allocator<V>> &t, node_base<V> *node,
                    int level) {
  for (int i = 0; i < level; ++i) {
    putchar('\t');
    putchar('\t');
  }

  printf("[%p] {", node);
  if (node->storage_valid_) {
    printf("value: {%s, %d}, ", node->get_value().first.c_str(),
           node->get_value().second);
    printf("prev: %p, next: %p, ", node->prev_, node->next_);
  }
  printf("subfix: {%lu, ", node->subfix_size_);
  for (std::size_t i = 0; i < node->subfix_size_; ++i) {
    putchar(node->subfix_start_[i]);
  }
  printf("}, ");

  printf("parent: %p, ", node->parent_);

  child_slot<V> slots[256];
  int s = node->get_all_children(slots);
  if (s) {
    printf("children: (%d){", s);
    for (int i = 0; i < s; ++i) {
      printf("{%c, %p}, ", slots[i].c, *slots[i].node);
    }
    printf("}, ");
  }
  printf("}\n");

  fflush(stdout);

  for (int i = 0; i < s; ++i) {
    print_preorder(t, *slots[i].node, level + 1);
  }
}

template <typename K, typename V>
void print(art_tree<K, V, std::allocator<V>> &t) {
  printf("\n--------------------------------------\n");

  printf("\n");

  printf("size: %lu, node count: %lu\n", t.impl_.size_, t.impl_.node_counter_);
  if (t.impl_.root_) {
    print_preorder(t, t.impl_.root_, 0);
  }

  {
    node_link_base *p = &t.impl_.dummy_;
    node_base<V> *n = static_cast<node_base<V> *>(p);
    printf("forward: [%p]{}", p);
    for (p = p->next_; p != &t.impl_.dummy_; p = p->next_) {
      n = static_cast<node_base<V> *>(p);
      printf(" --> [%p]{%s, %d}", n, n->get_value().first.c_str(),
             n->get_value().second);
    }
    printf("\n");
  }

  {
    node_link_base *p = &t.impl_.dummy_;
    node_base<V> *n = static_cast<node_base<V> *>(p);
    printf("backward: [%p]{}", p);
    for (p = p->prev_; p != &t.impl_.dummy_; p = p->prev_) {
      n = static_cast<node_base<V> *>(p);
      printf(" --> [%p]{%s, %d}", n, n->get_value().first.c_str(),
             n->get_value().second);
    }
    printf("\n");
  }

  printf("\n--------------------------------------\n");
}

#include <map>

string generate_rand_string() {
  static mt19937 rng;
  int s = (rng() % 32) + 1; // [1, 32]
  string str(s, '0');
  for (int j = 0; j < s; j++) {
    str[j] = (rng() % ('z' - 'A')) + 'A';
  }
  return str;
}

void stress_test() {
  mt19937 rng;
  map<string, int> m;
  art<string, int> t;

  for (int K = 0; K < 20; K++) {
    vector<string> v;

    // insert kv
    {
      for (int i = 0; i < 100000; i++) {
        string str = generate_rand_string();
        v.push_back(str);

        if (m.insert({str, i}).second != t.insert({str, i}).second) {
          throw "bad insert";
        }

        if (t.size() != m.size()) {
          throw "bad size";
        }
      }
    }

    // update kv
    {
      for (int i = 0; i < 100000; i++) {
        string str = generate_rand_string();
        v.push_back(str);

        m[str] = i;
        t[str] = i;

        if (m[str] != t[str]) {
          throw "bad at";
        }

        if (t.size() != m.size()) {
          throw "bad size";
        }
      }
    }

    // iterate kv
    {
      auto art_it = t.begin();
      auto it = m.begin();
      for (; it != m.end(); ++it, ++art_it) {
        if (art_it->first != it->first) {
          throw "bad key";
        }
        if (art_it->second != it->second) {
          throw "bad value";
        }
      }
      if (art_it != t.end()) {
        throw "bad end";
      }
    }

    // random search existed kv
    {
      random_shuffle(v.begin(), v.end(), [&](size_t n) { return rng() % n; });
      for (int i = 0; i < v.size(); i++) {
        int r1 = m.find(v[i])->second;
        int r2 = t.find(v[i])->second;

        if (r1 != r2) {
          throw "find bad";
        }
      }
    }

    // random search kv (maybe not exist)
    {
      for (int i = 0; i < 10000; i++) {
        string str = generate_rand_string();

        auto it1 = m.find(str);
        auto it2 = t.find(str);
        if (it1 == m.end()) {
          if (it2 != t.end()) {
            throw "found bad key";
          }
        } else if (it1->second != it2->second) {
          throw "found bad value";
        }
      }
    }

    // lower bound search
    {
      for (int i = 0; i < 10000; i++) {
        string str = generate_rand_string();

        auto it1 = m.lower_bound(str);
        auto it2 = t.lower_bound(str);
        if (it1 == m.end()) {
          if (it2 != t.end()) {
            throw "lower bound end";
          }
        } else if (it1->first != it2->first || it1->second != it2->second) {
          throw "lower bound kv";
        }
      }
    }

    // upper bound search
    {
      for (int i = 0; i < 10000; i++) {
        string str = generate_rand_string();

        auto it1 = m.upper_bound(str);
        auto it2 = t.upper_bound(str);
        if (it1 == m.end()) {
          if (it2 != t.end()) {
            throw "lower bound end";
          }
        } else if (it1->first != it2->first || it1->second != it2->second) {
          throw "lower bound kv";
        }
      }
    }

    // random erase existed kv (maybe repeated)
    {
      random_shuffle(v.begin(), v.end(), [&](size_t n) { return rng() % n; });
      for (int i = 0; i < v.size() / 2; i++) {
        int r1 = m.erase(v[i]);
        int r2 = t.erase(v[i]);

        if (r1 != r2) {
          throw "erase bad";
        }
      }

      if (t.size() != m.size()) {
        throw "bad size";
      }
    }
  }

  {
    vector<string> v;

    // iterate all
    {
      auto art_it = t.begin();
      auto it = m.begin();
      for (; it != m.end(); ++it, ++art_it) {
        if (art_it->first != it->first) {
          throw "bad key";
        }
        if (art_it->second != it->second) {
          throw "bad value";
        }
        v.push_back(it->first);
      }
      if (art_it != t.end()) {
        throw "bad end";
      }
    }

    // erase all
    {
      random_shuffle(v.begin(), v.end(), [&](size_t n) { return rng() % n; });

      for (int i = 0; i < v.size(); i++) {
        int r1 = m.erase(v[i]);
        int r2 = t.erase(v[i]);

        if (r1 != r2) {
          throw "erase bad";
        }
      }

      if (t.size() != m.size()) {
        throw "bad size";
      }
    }
  }
}

size_t n_ = 0;

template <typename T> struct my_allocator {
  using value_type = T;
  using pointer = T *;

  my_allocator() {}
  template <typename R> my_allocator(const my_allocator<R> &) {}

  T *allocate(size_t n) {
    n_ += n;
    return std::allocator<T>{}.template allocate(n);
  }

  void deallocate(T *ptr, size_t n) {
    n_ -= n;
    std::allocator<T>{}.template deallocate(ptr, n);
  }

  template <typename R> bool operator==(const my_allocator<R> &other) const {
    return true;
  }

  template <typename R> bool operator!=(const my_allocator<R> &other) const {
    return false;
  }
};

void ctor_test() {
  {
    map<string, int> m1, m2;
    art<string, int, my_allocator<int>> t1, t2;

    vector<pair<string, int>> v;

    for (int i = 0; i < 10000; i++) {
      string str = generate_rand_string();
      v.push_back({str, i});
    }

    m1 = map<string, int>(v.begin(), v.end());
    t1 = art<string, int, my_allocator<int>>(v.begin(), v.end());

    v.clear();

    for (int i = 0; i < 10000; i++) {
      string str = generate_rand_string();
      v.push_back({str, i});
    }

    m1.insert(v.begin(), v.end());
    t1.insert(v.begin(), v.end());

    {
      auto it = m1.begin();
      auto art_it = t1.begin();
      for (; it != m1.end(); ++it, ++art_it) {
        if (art_it->first != it->first) {
          throw "bad key";
        }
        if (art_it->second != it->second) {
          throw "bad value";
        }
      }
      if (art_it != t1.end()) {
        throw "bad end";
      }
    }

    {
      m2 = m1;
      t2 = t1;

      auto it = m2.begin();
      auto art_it = t2.begin();
      for (; it != m2.end(); ++it, ++art_it) {
        if (art_it->first != it->first) {
          throw "bad key";
        }
        if (art_it->second != it->second) {
          throw "bad value";
        }
      }
      if (art_it != t2.end()) {
        throw "bad end";
      }
    }

    {
      m2 = move(m1);
      t2 = move(t1);

      auto it = m2.begin();
      auto art_it = t2.begin();
      for (; it != m2.end(); ++it, ++art_it) {
        if (art_it->first != it->first) {
          throw "bad key";
        }
        if (art_it->second != it->second) {
          throw "bad value";
        }
      }
      if (art_it != t2.end()) {
        throw "bad end";
      }

      m2.clear();
      t2.clear();
    }
  }

  {
    if (n_ != 0) {
      throw "bad allocator";
    }
  }
}

void allocator_test() {
  using my_string = basic_string<char, char_traits<char>, my_allocator<char>>;

  {
    art<my_string, int, my_allocator<int>> t;
    for (int i = 0; i < 10000; i++) {
      string str = generate_rand_string();
      t.insert({my_string(str.c_str(), str.size()), i});
    }
  }

  {
    if (n_ != 0) {
      throw "bad allocator";
    }
  }
}

void performance_test() {
  constexpr int NUM_ELEMENTS = 1000000;

  std::map<std::string, int> myMap;
  art<string, int> myArt;

  auto ptest = [](auto &my) {
    auto perf_op = [](const string &name, auto &&fn) {
      auto s = std::chrono::high_resolution_clock::now();
      fn();
      auto e = std::chrono::high_resolution_clock::now();
      printf(
          "%s: %lu ms\n", name.c_str(),
          std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count());
    };

    {
      vector<pair<string, int>> v;

      for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
        std::string key = generate_rand_string();
        v.push_back({key, i});
      }

      perf_op("Insertion time", [&]() {
        for (auto it : v) {
          my.insert(it);
        }
      });
    }

    // Find test
    {
      std::string findKey = generate_rand_string();
      my[findKey] = NUM_ELEMENTS; // 确保能找到
      perf_op("Find Time", [&]() {
        for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
          auto it = my.find(findKey);
        }
      });
    }

    // Erase test
    {
      perf_op("Erase time", [&]() {
        for (auto it = my.begin(); it != my.end();) {
          if (it->second % 2 == 0) {
            it = my.erase(it);
          } else {
            ++it;
          }
        }
      });
    }

    // Lower bound test
    {
      vector<string> v;

      for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
        std::string key = generate_rand_string();
        v.push_back(key);
      }

      perf_op("Lower bound time", [&]() {
        for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
          auto it = my.lower_bound(v[i]);
        }
      });
    }

    // Iterator traversal
    {
      perf_op("Traversal time", [&]() {
        for (auto it = my.begin(); it != my.end(); ++it) {
          // No-op to simulate work
        }
      });
    }
  };

  printf("std::map: \n");
  ptest(myMap);
  printf("art: \n");
  ptest(myArt);
}

int main() {
  stress_test();
  ctor_test();
  allocator_test();
  performance_test();

  return 0;
}
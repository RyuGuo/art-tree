#include "art.h"
#include <random>
#include <sys/time.h>
#include <unordered_map>
#include <unordered_set>

uint64_t get_ts() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec * 1e6 + tv.tv_usec;
}

int main() {

  if (0) {
    art_set s;

    s.insert("aab", 3);
    s._m_dump();
    s.insert("aabb", 4);
    s._m_dump();
    s.insert("abc", 3);
    s._m_dump();
    s.insert("abcd", 4);
    s._m_dump();
    s.insert("ab", 2);
    s._m_dump();
    s.insert("abc", 3);
    s._m_dump();
    s.insert("baaaa", 5);
    s._m_dump();
    s.insert("baac", 4);
    s._m_dump();
    s.insert("baa", 3);
    s._m_dump();
    s.insert("baada", 5);
    s._m_dump();
    s.insert("baaea", 5);
    s._m_dump();
    s.insert("baafa", 5);
    s._m_dump();
    s.insert("baaga", 5);
    s._m_dump();
    s.insert("baaia", 5);
    s._m_dump();
    s.insert("baaha", 5);
    s._m_dump();
    s.insert("baahaaa", 7);
    s._m_dump();
    s.insert("bachaaa", 7);
    s._m_dump();
    s.insert("bac", 3);
    s._m_dump();
    std::string str(1024, 'c');
    s.insert(str.data(), str.size());
    s._m_dump();
    str[1020] = 'a';
    s.insert(str.data(), str.size());
    s._m_dump();
    str = std::string(1025, 'c');
    for (int i = 0; i < 50; ++i) {
      str[1024] = 'A' + i;
      s.insert(str.data(), str.size());
      s._m_dump();
    }

    s.erase("a", 1);
    s._m_dump();
    s.erase("ab", 2);
    s._m_dump();
    s.erase("baa", 3);
    s._m_dump();
    s.erase("abcd", 4);
    s._m_dump();

    str = std::string(1024, 'c');
    s.erase(str.data(), str.size());
    s._m_dump();
    str[1020] = 'a';
    s.erase(str.data(), str.size());
    s._m_dump();
    str = std::string(1025, 'c');
    for (int i = 0; i < 50; ++i) {
      str[1024] = 'A' + i;
      s.erase(str.data(), str.size());
      s._m_dump();
    }
  }

  if (0) {
    art_set s;

    std::unordered_set<std::string> us;
    std::vector<std::string> v;
    std::mt19937 rng;

    for (int i = 0; i < 10000; ++i) {
      int len = (rng() % 256) + 1;
      std::string str(len, 0);
      for (int j = 0; j < len; ++j) {
        uint64_t r = rng();
        str[j] =
            ((uint64_t)(pow(r, 30) / pow(UINT32_MAX, 29)) % ('z' - 'A' + 1)) +
            'A';
      }
      v.emplace_back(str);
    }

    uint64_t st = get_ts();
    for (auto &str : v) {
      us.insert(str);
    }
    printf("unordered map insert: %f ms\n", (get_ts() - st) / 1e3);
    st = get_ts();
    for (auto &str : v) {
      s.insert(str.data(), str.size());
    }
    printf("art insert: %f ms\n", (get_ts() - st) / 1e3);

    // s._m_dump();

    st = get_ts();
    for (auto &str : v) {
      assert(us.find(str) != us.end());
    }
    printf("unordered map find: %f ms\n", (get_ts() - st) / 1e3);
    st = get_ts();
    for (auto &str : v) {
      assert(s.find(str.data(), str.size()) == true);
    }
    printf("art find: %f ms\n", (get_ts() - st) / 1e3);

    for (int i = 0; i < 10000; ++i) {
      int len = (rng() % 256) + 1;
      std::string str(len, 0);
      for (int j = 0; j < len; ++j) {
        str[j] = (rng() % ('z' - 'A' + 1)) + 'A';
      }
      assert(s.find(str.data(), len) == (us.find(str) != us.end()));
    }
  }

  if (1) {
    art_map<int> s;

    s.insert("aab", 3, 0);
    s._m_dump();
    s.insert("aabb", 4, 1);
    s._m_dump();
    s.insert("abc", 3, 2);
    s._m_dump();
    s.insert("abcd", 4, 3);
    s._m_dump();
    s.insert("ab", 2, 4);
    s._m_dump();
    s.insert("abc", 3, 5);
    s._m_dump();
    s.insert("baaaa", 5, 6);
    s._m_dump();
    s.insert("baac", 4, 7);
    s._m_dump();
    s.insert("baa", 3, 8);
    s._m_dump();
    s.insert("baada", 5, 9);
    s._m_dump();
    s.insert("baaea", 5, 10);
    s._m_dump();
    s.insert("baafa", 5, 11);
    s._m_dump();
    s.insert("baaga", 5, 12);
    s._m_dump();
    s.insert("baaia", 5, 13);
    s._m_dump();
    s.insert("baaha", 5, 14);
    s._m_dump();
    s.insert("baahaaa", 7, 15);
    s._m_dump();
    s.insert("bachaaa", 7, 16);
    s._m_dump();
    s.insert("bac", 3, 17);
    s._m_dump();
    std::string str(1024, 'c');
    s.insert(str.data(), str.size(), 18);
    s._m_dump();
    str[1020] = 'a';
    s.insert(str.data(), str.size(), 19);
    s._m_dump();
    str = std::string(1025, 'c');
    for (int i = 0; i < 50; ++i) {
      str[1024] = 'A' + i;
      s.insert(str.data(), str.size(), i);
      s._m_dump();
    }

    s.erase("a", 1);
    s._m_dump();
    s.erase("ab", 2);
    s._m_dump();
    s.erase("baa", 3);
    s._m_dump();
    s.erase("abcd", 4);
    s._m_dump();

    str = std::string(1024, 'c');
    s.erase(str.data(), str.size());
    s._m_dump();
    str[1020] = 'a';
    s.erase(str.data(), str.size());
    s._m_dump();
    str = std::string(1025, 'c');
    for (int i = 0; i < 50; ++i) {
      str[1024] = 'A' + i;
      s.erase(str.data(), str.size());
      s._m_dump();
    }
  }

  if (1) {
    art_map<std::string> s;

    s.insert("aab", 3, "0");
    s._m_dump();
    s.insert("aabb", 4, "1");
    s._m_dump();
    s.insert("abc", 3, "2");
    s._m_dump();
    s.insert("abcd", 4, "3");
    s._m_dump();
    s.insert("ab", 2, "4");
    s._m_dump();
    s.insert("abc", 3, "5");
    s._m_dump();
    s.insert("baaaa", 5, "6");
    s._m_dump();
    s.insert("baac", 4, "7");
    s._m_dump();
    s.insert("baa", 3, "8");
    s._m_dump();
    s.insert("baada", 5, "9");
    s._m_dump();
    s.insert("baaea", 5, "10");
    s._m_dump();
    s.insert("baafa", 5, "11");
    s._m_dump();
    s.insert("baaga", 5, "12");
    s._m_dump();
    s.insert("baaia", 5, "13");
    s._m_dump();
    s.insert("baaha", 5, "14");
    s._m_dump();
    s.insert("baahaaa", 7, "15");
    s._m_dump();
    s.insert("bachaaa", 7, "16");
    s._m_dump();
    s.insert("bac", 3, "17");
    s._m_dump();
    std::string str(1024, 'c');
    s.insert(str.data(), str.size(), "18");
    s._m_dump();
    str[1020] = 'a';
    s.insert(str.data(), str.size(), "19");
    s._m_dump();
    str = std::string(1025, 'c');
    for (int i = 0; i < 50; ++i) {
      str[1024] = 'A' + i;
      s.insert(str.data(), str.size(), std::to_string(i));
      s._m_dump();
    }

    s.erase("a", 1);
    s._m_dump();
    s.erase("ab", 2);
    s._m_dump();
    s.erase("baa", 3);
    s._m_dump();
    s.erase("abcd", 4);
    s._m_dump();

    str = std::string(1024, 'c');
    s.erase(str.data(), str.size());
    s._m_dump();
    str[1020] = 'a';
    s.erase(str.data(), str.size());
    s._m_dump();
    str = std::string(1025, 'c');
    for (int i = 0; i < 50; ++i) {
      str[1024] = 'A' + i;
      s.erase(str.data(), str.size());
      s._m_dump();
    }
  }

  if (1) {
    const int IT = 10000;
    art_map<std::string> s;

    std::unordered_map<std::string, std::string> um;
    std::vector<std::pair<std::string, std::string>> v;
    std::mt19937 rng;

    for (int i = 0; i < IT; ++i) {
      int len = (rng() % 256) + 1;
      std::string str(len, 0);
      for (int j = 0; j < len; ++j) {
        uint64_t r = rng();
        str[j] =
            ((uint64_t)(pow(r, 30) / pow(UINT32_MAX, 29)) % ('z' - 'A' + 1)) +
            'A';
      }
      str += std::to_string(i);
      v.emplace_back(str, std::to_string(rng()));
    }

    uint64_t st = get_ts();
    for (auto &p : v) {
      um.emplace(p.first, p.second);
    }
    printf("unordered map insert: %f us\n", (get_ts() - st) / 1e0 / IT);
    st = get_ts();
    for (auto &p : v) {
      s.insert(p.first.data(), p.first.size(), p.second);
    }
    printf("art insert: %f us\n", (get_ts() - st) / 1e0 / IT);

    // s._m_dump();

    st = get_ts();
    for (auto &p : v) {
      auto res = um.find(p.first);
      assert(res != um.end() && res->second == p.second);
    }
    printf("unordered map find: %f us\n", (get_ts() - st) / 1e0 / IT);
    st = get_ts();
    for (auto &p : v) {
      auto res = s.find(p.first.data(), p.first.size());
      assert(res.first == true && *res.second == p.second);
    }
    printf("art find: %f us\n", (get_ts() - st) / 1e0 / IT);

    for (int i = 0; i < 10000; ++i) {
      int len = (rng() % 256) + 1;
      std::string str(len, 0);
      for (int j = 0; j < len; ++j) {
        str[j] = (rng() % ('z' - 'A' + 1)) + 'A';
      }
      assert(s.find(str.data(), len).first == (um.find(str) != um.end()));
    }
  }

  return 0;
}
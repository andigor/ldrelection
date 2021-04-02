#include <iostream>


#include <thread>
#include <mutex>
#include <barrier>
#include <atomic>
#include <cassert>

class server
{
public:
  enum class state {
      unknown
    , standby
    , duty
  };
  void set_other_server(server* o)
  {
    m_other_server = o;
  }
  void reset()
  {
    m_id = 0;
    m_st = state::unknown;
  }
  bool set_id()
  {
    uint32_t expected = 0;
    auto ret = m_id.compare_exchange_strong(expected, 1);
    return ret;
  }
  bool run_election()
  {
    bool ret = m_other_server->set_id();
    if (ret) {
      bool ret2 = set_id();
      if (ret2) {
        m_st = state::duty;
      }
      else {
        m_id = 0;
        ++m_count;
      }
    }
    else {
      m_st = state::standby;
    }
    return m_st != state::unknown;
  }
  bool is_elected() const
  {
    return m_st != state::unknown;
  }
  state get_state() const
  {
    return m_st;
  }
  size_t get_count() const
  {
    return m_count;
  }
private:
  std::atomic_uint32_t m_id = 0;
  server* m_other_server = nullptr;
  state m_st = state::unknown;
  size_t m_count = 0;
};

int main()
{
  //for (size_t i =0; i<10000; ++i)
  //{
    server s1;
    server s2;

    s1.set_other_server(&s2);
    s2.set_other_server(&s1);

    std::barrier bar(2);
    std::thread thr1(
      [&s1, &s2, &bar] {
        for (size_t i = 0; i<1000000; ++i) {
          for (;;) {
            auto res = s1.run_election();
            if (res) {
              bar.arrive_and_wait();
              break;
            }
          }
          // just check
          if (!s1.is_elected()) {
            throw std::exception("1");
          }
          if (!s2.is_elected()) {
            throw std::exception("2");
          }
          if (s1.get_state() == s2.get_state()) {
            throw std::exception("3");
          }
          bar.arrive_and_wait();
          s1.reset();
          bar.arrive_and_wait();
        }
      }
    );

    std::thread thr2(
      [&s1, &s2, &bar] {
        for (size_t i = 0; i < 1000000; ++i) {
          for (;;) {
            auto res = s2.run_election();
            if (res) {
              bar.arrive_and_wait();
              break;
            }
          }

          // just check
          if (!s1.is_elected()) {
            throw std::exception("1");
          }
          if (!s2.is_elected()) {
            throw std::exception("2");
          }
          if (s1.get_state() == s2.get_state()) {
            throw std::exception("3");
          }
          bar.arrive_and_wait();
          s2.reset();
          bar.arrive_and_wait();
        }
      }
    );

    thr1.join();
    thr2.join();
  //}
    std::cout << "count1: " << s1.get_count() << " count2: " << s2.get_count() << std::endl;
}


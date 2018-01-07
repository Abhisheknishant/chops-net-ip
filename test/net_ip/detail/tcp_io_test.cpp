/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_io detail class.
 *
 *  The test strategy is to create mock net entity classes to test the interface
 *  and functionality of the @c tcp_io class. In particular, the notification hooks
 *  between the two levels is tricky and needs to be well tested.
 *
 *  The data flow is tested by sending a group of messages (variable len, text). 
 *  optionally the message handler will send them back to the originator.
 *
 *  The data flow is terminated by a message with an empty body.
 *  
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>
#include <experimental/executor>

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move
#include <tuple>
#include <thread>
#include <future>
#include <chrono>
#include <functional>

#include "net_ip/detail/tcp_io.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"

using namespace std::experimental::net;
using namespace chops::test;

using wk_guard = executor_work_guard<io_context::executor_type>;

using notifier_cb = 
  typename chops::net::detail::io_common<chops::net::detail::tcp_io>::entity_notifier_cb;

constexpr int test_port = 30434;
const char*   test_addr = "127.0.0.1";
constexpr int NumMsgs = 50;
constexpr int Interval = 50;


using notifier_data = std::tuple<std::error_code, std::shared_ptr<chops::net::detail::tcp_io> >;

struct entity_notifier {
  entity_notifier() = default;

  std::promise<notifier_data> prom;

  void notify_me(const std::error_code& e, std::shared_ptr<chops::net::detail::tcp_io> p) {
    prom.set_value(notifier_data(e, p));
  }

};

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread; return data: 1. Error code 2. Does the ioh ptr match? 3. Do the msg sets match?
using thread_data = std::tuple<std::error_code, bool, bool>; 
using thread_promise = std::promise<thread_data>;

using tcp_ioh_ptr = std::shared_ptr<chops::net::detail::tcp_io>;

auto create_start_iohp(ip::tcp::socket sock, entity_notifier& en, bool reply) {
  using namespace std::placeholders;

  notifier_cb cb(std::bind(&entity_notifier::notify_me, &en, _1, _2));

  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(sock), cb);

  msg_hdlr<chops::net::detail::tcp_io> mh (reply);
  iohp->start_io(mh, variable_len_msg_frame, 2);
  
  return iohp;
}

struct test_acceptor {
  ip::tcp::acceptor m_acc;
  tcp_ioh_ptr       m_ioh_ptr;
  entity_notifier   m_en;
  bool              m_reply;

  test_acceptor(io_context& ioc, bool reply) : 
           m_acc(ioc, ip::tcp::endpoint(ip::address_v4::any(), test_port)), 
           m_ioh_ptr(), m_en(), m_reply(reply) { }

  void start() {
    m_acc.async_accept([this] (const std::error_code& err, ip::tcp::socket sock) {
      auto en_fut = m_en.prom.get_future();
      auto iohp = create_start_iohp(std::move(sock), m_en, m_reply);
  }

};

void acceptor_func (thread_promise thr_prom, const vec_buf& in_msg_set, io_context& ioc, 
                    bool reply) {

  test_acceptor acc(ioc);
  acc.start();

  auto ret = en_fut.get(); // wait for termination callback

  thr_prom.set_value(thread_data(std::get<0>(ret), std::get<1>(ret) == iohp, in_msg_set == mh.msgs));

}

void connector_func (thread_promise thr_prom, const vec_buf& in_msg_set, io_context& ioc, 
                     int interval) {
  using namespace std::placeholders;

  entity_notifier en { };
  auto en_fut = en.prom.get_future();
  notifier_cb cb(std::bind(&entity_notifier::notify_me, &en, _1, _2));

  ip::tcp::socket sock(ioc);
  sock.connect(ip::tcp::endpoint(ip::make_address(test_addr), test_port));
  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(sock), cb);

  msg_hdlr<chops::net::detail::tcp_io> mh (false);

  iohp->start_io(mh, variable_len_msg_frame, 2);

  for (auto buf : in_msg_set) {
    iohp->send(chops::const_shared_buffer(std::move(buf)));
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  // send empty body, shutdown signal
  iohp->send(chops::const_shared_buffer(make_variable_len_msg(chops::mutable_shared_buffer())));
  auto ret = en_fut.get(); // wait for termination callback

  thr_prom.set_value(thread_data(std::get<0>(ret), std::get<1>(ret) == iohp, in_msg_set == mh.msgs));

}




SCENARIO ( "Tcp IO handler test, one-way", "[tcp_io_one_way]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);

  io_context ioc;
  wk_guard wg { make_work_guard(ioc) };
  std::thread run_thr([&ioc] () { ioc.run(); } );

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and connector are created") {
      thread_promise acc_prom { };
      auto acc_fut = acc_prom.get_future();
      std::thread acc_thr(acceptor_func, std::move(acc_prom), std::cref(ms), std::ref(ioc), 
                          false);
      thread_promise conn_prom { };
      auto conn_fut = conn_prom.get_future();
      std::thread conn_thr(connector_func, std::move(conn_prom), std::cref(ms), std::ref(ioc), 
                          Interval);
      THEN ("the futures provide synchronization and data returns") {
        auto acc_data = acc_fut.get();
        acc_thr.join();
        auto conn_data = conn_fut.get();
        conn_thr.join();
        INFO ("Acceptor error code: " << std::get<0>(acc_data));
        INFO ("Connector error code: " << std::get<0>(acc_data));
        REQUIRE (std::get<1>(acc_data));
        REQUIRE (std::get<1>(conn_data));
        REQUIRE (std::get<2>(acc_data));
        REQUIRE (std::get<2>(conn_data));
      }
    }
  } // end given
  wg.reset();
  run_thr.join();
}

/*
SCENARIO ( "Io common test, tcp", "[io_common_tcp]" ) {
}
*/

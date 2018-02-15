/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenario for @c io_interface delivery functions.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <chrono>
#include <thread>
#include <memory>
#include <future>

#include "net_ip/component/io_interface_delivery.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"


struct io_mock {
  using socket_type = int;
  using endpoint_type = long;

  socket_type& get_socket() { ++magic; return magic; }

  int magic = 41;
};

using io_mock_ptr = std::shared_ptr<io_mock>;
using io_interface_mock = chops::net::basic_io_interface<io_mock>;

struct entity_mock {

  using socket_type = int;
  using endpoint_type = double;

  io_mock_ptr  iop;
  std::thread  thr;
  
  entity_mock() : iop(std::make_shared<io_mock>()) { }

  void join_thr() { thr.join(); }

  template <typename F>
  void start(F&& state_chg_func) {
    thr = std::thread([ this, state_chg_func ] () mutable {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        state_chg_func(io_interface_mock(iop), 1, true);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        state_chg_func(io_interface_mock(iop), 0, false);
      }
    );
  }

  template <typename F1, typename F2>
  void start(F1&& state_chg_func, F2&& err_func ) {
    thr = std::thread([ this, state_chg_func, err_func ] () mutable {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        state_chg_func(io_interface_mock(iop), 1, true);
        err_func(io_interface_mock(iop), 
                 std::make_error_code(chops::net::net_ip_errc::message_handler_terminated));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        state_chg_func(io_interface_mock(iop), 0, false);
      }
    );
  }
};

using net_entity_mock = chops::net::basic_net_entity<entity_mock>;

SCENARIO ( "Testing make_io_interface_future_impl",
           "[io_interface_future]" ) {

  GIVEN ("A entity object") {
    auto ent_ptr = std::make_shared<entity_mock>();
    auto ent = net_entity_mock (ent_ptr);
    WHEN ("make_io_interface_future_impl is called") {
      auto fut = 
        chops::net::detail::make_io_interface_future_impl<io_mock>(ent);

      THEN ("a future is returned, which returns a value when ready") {
          auto io = fut.get();
          REQUIRE (io.get_socket() == 42);
          ent_ptr->join_thr();
      }
    }
    AND_WHEN ("make_io_interface_future_pair_impl is called") {
      auto pair_fut = 
        chops::net::detail::make_io_interface_future_pair_impl<io_mock>(ent);

      THEN ("two futures are returned, both of which return a value when ready") {
          auto io1 = pair_fut.start_fut.get();
          REQUIRE (io1.get_socket() == 42);
          auto io2 = pair_fut.stop_fut.get();
          REQUIRE (io2.get_socket() == 43);
          ent_ptr->join_thr();
      }
    }
  } // end given
}

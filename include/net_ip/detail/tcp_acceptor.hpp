/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief TCP acceptor, for internal use.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2018
 *
 */

#ifndef TCP_ACCEPTOR_HPP_INCLUDED
#define TCP_ACCEPTOR_HPP_INCLUDED

#include <experimental/internet>
#include <experimental/io_context>

#include <system_error>
#include <memory>

#include <cstddef> // for std::size_t

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_base.hpp"


namespace chops {
namespace net {
namespace detail {

class tcp_acceptor : public std::enable_shared_from_this<tcp_acceptor> {
private:
  net_entity_base<tcp_io>                     m_entity_base;
  std::experimental::net::ip::tcp::acceptor   m_acceptor;
  std::experimental::net::ip::tcp::endpoint   m_acceptor_endp;
  bool                                        m_reuse_addr;
  std::experimental::net::ip::tcp::endpoint   m_remote_endp;

public:
  tcp_acceptor(std::experimental::net::io_context& ioc, 
               const std::experimental::net::ip::tcp::endpoint& endp,
               bool reuse_addr) :
    m_entity_base(), m_acceptor(ioc), m_acceptor_endp(endp), 
    m_reuse_addr(reuse_addr), m_remote_endp() { }

public:
  bool is_started() const noexcept { return m_entity_base.is_started(); }

  template <typename F>
  void start(F&& state_chg) {
    if (!m_entity_base.start(std::forward<F>(state_chg))) { // already started
      return;
    }
    try {
      m_acceptor = std::experimental::net::ip::tcp::acceptor(m_ioc, m_acceptor_endp, m_reuse_addr);
    }
    catch (const std::system_error& se) {
      m_entity_base.call_state_change_cb(se.code(), tcp_io_ptr());
      m_entity_base.stop();
      return;
    }
    start_accept();
  }

private:

  void start_accept() {
    m_acceptor.async_accept(m_remote_endp, 
                            [] (const std::error_code& err,
                                std::experimental::net::ip::tcp::socket sock) {
                                  handle_accept(err, sock);
                                }
  }

  void handle_accept(const std::error_code&, 

};

} // end detail namespace
} // end net namespace
} // end chops namespace


#endif


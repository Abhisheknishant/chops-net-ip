/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Common code, factored out, for TCP and UDP io handlers.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef IO_COMMON_HPP_INCLUDED
#define IO_COMMON_HPP_INCLUDED

#include <optional>
#include <mutex>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/queue_stats.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename E>
class io_common {
private:
  bool                m_io_started; // original implementation this was std::atomic_bool
  bool                m_write_in_progress;
  output_queue<E>     m_outq;
  mutable std::mutex  m_mutex;

private:
  using lk_guard = std::lock_guard<std::mutex>;

public:
  enum write_status { io_stopped, queued, write_started };

public:

  io_common() noexcept :
    m_io_started(false), m_write_in_progress(false), m_outq(), m_mutex() { }

  // the following four methods can be called concurrently
  auto get_output_queue_stats() const noexcept {
    lk_guard lg(m_mutex);
    return m_outq.get_queue_stats();
  }

  bool is_io_started() const noexcept {
    lk_guard lg(m_mutex);
    return m_io_started;
  }

  bool set_io_started() noexcept {
    lk_guard lg(m_mutex);
    return m_io_started ? false : m_io_started = true, true;
  }

  bool set_io_stopped() noexcept {
    lk_guard lg(m_mutex);
    return m_io_started ? m_io_started = false, true : false;
  }

  // rest of these method called only from within run thread
  bool is_write_in_progress() const noexcept {
    lk_guard lg(m_mutex);
    return m_write_in_progress;
  }

  template <typename F>
  write_status start_write(const E&, F&&);

  template <typename F>
  void write_next_elem(F&&);

  void clear() noexcept {
    lk_guard lg(m_mutex);
    m_outq.clear();
    m_write_in_progress = false;
  }

};

template <typename E, typename F>
auto io_common<E>::start_write(const E& elem, F&& func) {
  lk_guard lg(m_mutex);
  if (!m_io_started) {
    clear();
    return io_stopped; // shutdown happening or not io_started, don't start a write
  }
  if (m_write_in_progress) { // queue buffer
    m_outq.add_element(elem);
    return queued;
  }
  func(elem);
  m_write_in_progress = true;
  return write_started;
}

template <typename E, typename F>
void io_common<IOT>::write_next_elem(F&& func) {
  lk_guard lg(m_mutex);
  if (!m_io_started) { // shutting down
    clear();
    return;
  }
  auto elem = m_outq.get_next_element();
  if (!elem) {
    m_write_in_progress = false;
    return;
  }
  func(elem);
  m_write_in_progress = true;
  return;
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif


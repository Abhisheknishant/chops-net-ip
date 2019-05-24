/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c basic_io_output class template.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <memory> // std::shared_ptr
#include <set>
#include <cstddef> // std::size_t

#include "net_ip/queue_stats.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"

#include "marshall/shared_buffer.hpp"
#include "utility/make_byte_array.hpp"

#include "shared_test/mock_classes.hpp"

template <typename IOT>
void basic_io_output_test_construction_and_release() {

  GIVEN ("A default constructed basic_io_output") {
    chops::net::basic_io_output<IOT> io_out { };
    WHEN ("is_valid is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (io_out.is_valid());
      }
    }
  } // end given

  GIVEN ("A basic_io_interface constructed with an io handler") {
    auto ioh = std::make_shared<IOT>();
    chops::net::basic_io_interface<IOT> io_intf(ioh);
    WHEN ("make_io_output is called on the basic_io_interface") {
      auto io_out = io_intf.make_io_output();
      THEN ("a valid basic_io_output object is created and in a non-started state") {
        REQUIRE (io_out);
        REQUIRE(io_out->is_valid());
        REQUIRE_FALSE(io_out->is_io_started());
      }
    }
  } // end given

  GIVEN ("An io handler shared pointer") {
    auto ioh = std::make_shared<IOT>();
    WHEN ("Two basic_io_outputs are constructed, from the io handler shared pointer and raw pointer") {
      chops::net::basic_io_output<IOT> io_out1(ioh);
      chops::net::basic_io_output<IOT> io_out2(ioh.get());
      THEN ("both are valid") {
        REQUIRE(io_out1.is_valid());
        REQUIRE(io_out2.is_valid());
      }
      AND_THEN ("both can be released and are no longer valid") {
        io_out1.release();
        io_out2.release();
        REQUIRE_FALSE(io_out1.is_valid());
        REQUIRE_FALSE(io_out2.is_valid());
      }
    }
  } // end given
}

template <typename IOT>
void basic_io_output_test_sends() {

  auto ioh = std::make_shared<IOT>();
  chops::net::basic_io_output<IOT> io_out(ioh);
  REQUIRE (io_out.is_valid());
  REQUIRE_FALSE (io_out.is_io_started());
  ioh->start_io();

  GIVEN ("A basic_io_output associated with an io handler that has been started") {
    WHEN ("is_io_started or get_output_queue_stats is called") {
      THEN ("correct values are returned") {
        REQUIRE(io_out.is_io_started());
        chops::net::output_queue_stats s = io_out.get_output_queue_stats();
        REQUIRE (s.output_queue_size == chops::test::io_handler_mock::qs_base);
        REQUIRE (s.bytes_in_output_queue == (chops::test::io_handler_mock::qs_base + 1));
      }
    }
    AND_WHEN ("send is called") {
      THEN ("appropriate values are set or returned") {

        chops::const_shared_buffer buf(nullptr, 0);
        using endp_t = typename IOT::endpoint_type;

        REQUIRE (io_out.is_valid());

        io_out.send(nullptr, 0);
        io_out.send(buf);
        io_out.send(chops::mutable_shared_buffer());
        io_out.send(nullptr, 0, endp_t());
        io_out.send(buf, endp_t());
        io_out.send(chops::mutable_shared_buffer(), endp_t());
        REQUIRE(ioh->send_called);

      }
    }
  } // end given

}

template <typename IOT>
void basic_io_output_test_compare() {

  chops::net::basic_io_output<IOT> io_intf1 { };

  auto ioh1 = std::make_shared<IOT>();
  chops::net::basic_io_output<IOT> io_intf2(ioh1);

  chops::net::basic_io_output<IOT> io_intf3 { };

  auto ioh2 = std::make_shared<IOT>();
  chops::net::basic_io_output<IOT> io_intf4(ioh2);

  chops::net::basic_io_output<IOT> io_intf5 { };

  GIVEN ("Three default constructed basic_io_outputs and two with io handlers") {
    WHEN ("all are inserted in a set") {
      std::set<chops::net::basic_io_output<IOT> > a_set { io_intf1, io_intf2, io_intf3, io_intf4, io_intf5 };
      THEN ("the invalid basic_io_outputs are first in the set") {
        REQUIRE (a_set.size() == 5);
        auto i = a_set.cbegin();
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE (i->is_valid());
        ++i;
        REQUIRE (i->is_valid());
      }
    }
    AND_WHEN ("two invalid basic_io_outputs are compared for equality") {
      THEN ("they compare equal") {
        REQUIRE (io_intf1 == io_intf3);
        REQUIRE (io_intf3 == io_intf5);
      }
    }
    AND_WHEN ("two valid basic_outputs are compared for equality") {
      THEN ("they compare equal if both point to the same io handler") {
        REQUIRE_FALSE (io_intf2 == io_intf4);
        io_intf2 = io_intf4;
        REQUIRE (io_intf2 == io_intf4);
      }
    }
    AND_WHEN ("an invalid basic_output is order compared with a valid basic_io_output") {
      THEN ("the invalid compares less than the valid") {
        REQUIRE (io_intf1 < io_intf2);
      }
    }
  } // end given

}

SCENARIO ( "Basic io output test, io_handler_mock used for IO handler type",
           "[basic_io_output] [io_handler_mock]" ) {
  basic_io_output_test_construction_and_release<chops::test::io_handler_mock>();
  basic_io_output_test_sends<chops::test::io_handler_mock>();
}


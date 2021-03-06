/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c wp_access and @c wp_access_void function templates.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <memory> // std::shared_ptr, std::weak_ptr
#include <cstddef> // std::size_t

#include "nonstd/expected.hpp"

#include "shared_test/mock_classes.hpp"

#include "net_ip/detail/wp_access.hpp"

SCENARIO ( "Weak pointer access utility functions testing",
           "[wp_access]" ) {

  using ne_wp = std::weak_ptr<chops::test::net_entity_mock>;
  using ne_sp = std::shared_ptr<chops::test::net_entity_mock>;

  ne_wp empty_wp;

  GIVEN ("An empty weak pointer") {
    WHEN ("is_started is called, returning bool") {
      THEN ("the return value contains an error") {
        auto r = chops::net::detail::wp_access<bool>(empty_wp, [] (ne_sp nesp) { return nesp->is_started(); } );
        REQUIRE_FALSE (r);
        INFO("Error code: " << r.error());
      }
    }
    AND_WHEN ("the wp_access_void function is called") {
      THEN ("the return value contains an error") {
        auto r = chops::net::detail::wp_access_void(empty_wp, [] (ne_sp) { return std::error_code(); } );
        REQUIRE_FALSE (r);
        INFO("Error code: " << r.error());
      }
    }
  } // end given

  auto sp = std::make_shared<chops::test::net_entity_mock>();
  ne_wp wp(sp);

  GIVEN ("A weak pointer to a default constructed net_entity_mock") {
    WHEN ("is_started is called") {
      THEN ("the return value is false") {
        auto r = chops::net::detail::wp_access<bool>(wp, [] (ne_sp nesp) { return nesp->is_started(); } );
        REQUIRE (r);
        REQUIRE_FALSE (*r);
      }
    }
    AND_WHEN ("start is called followed by is_started followed by stop") {
      THEN ("all calls succeed") {
        auto r1 = chops::net::detail::wp_access_void(wp,
            [] (ne_sp nesp) { return nesp->start(chops::test::io_state_chg_mock, chops::test::err_func_mock ); }
        );
        REQUIRE (r1);
        auto r2 = chops::net::detail::wp_access<bool>(wp, [] (ne_sp nesp) { return nesp->is_started(); } );
        REQUIRE (r2);
        REQUIRE(*r2);
        auto r3 = chops::net::detail::wp_access_void(wp, [] (ne_sp nesp) { return nesp->stop(); } );
        REQUIRE (r3);
        auto r4 = chops::net::detail::wp_access<bool>(wp, [] (ne_sp nesp) { return nesp->is_started(); } );
        REQUIRE (r4);
        REQUIRE_FALSE(*r4);
      }
    }
    AND_WHEN ("visit_io_output is called") {
      THEN ("the call succeeds with the correct return value") {
        auto r = chops::net::detail::wp_access<std::size_t>(wp,
            [] (ne_sp nesp) { return nesp->visit_io_output(
                [] (chops::net::basic_io_output<chops::test::io_handler_mock>) { } ); 
            }
        );
        REQUIRE (r);
        REQUIRE (*r == 1u);
      }
    }
  } // end given

}



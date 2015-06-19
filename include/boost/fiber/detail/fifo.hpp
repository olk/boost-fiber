
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FIBERS_DETAIL_FIFO_H
#define BOOST_FIBERS_DETAIL_FIFO_H

#include <algorithm>
#include <cstddef>

#include <boost/config.hpp>

#include <boost/fiber/detail/config.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

class fiber_context;

namespace detail {

class BOOST_FIBERS_DECL fifo {
public:
    fifo() noexcept :
        head_( nullptr),
        tail_( & head_) {
    }

    fifo( fifo const&) = delete;
    fifo & operator=( fifo const&) = delete;

    bool empty() const noexcept {
        return nullptr == head_;
    }

    void push( fiber_context * item) noexcept;

    fiber_context * pop() noexcept;

    void swap( fifo & other) {
        std::swap( head_, other.head_);
        std::swap( tail_, other.tail_);
    }

private:
    fiber_context   *   head_;
    // tail_ points to the nxt field that contains the null that marks the end
    // of the fifo. When the fifo is empty, tail_ points to head_. tail_ must
    // never be null: it always points to a real fiber_context*. However, in
    // normal use, (*tail_) is always null.
    fiber_context   **  tail_;
};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBERS_DETAIL_FIFO_H

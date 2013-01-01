//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_FIBERS_SOURCE

#include <boost/fiber/round_robin.hpp>

#include <memory>
#include <utility>

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>

#include <boost/fiber/exceptions.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

#define RESUME_FIBER( f_) \
    BOOST_ASSERT( f_); \
    BOOST_ASSERT( ! f_->is_terminated() ); \
    f_->set_running(); \
    f_->resume(); \

namespace boost {
namespace fibers {

round_robin::round_robin() :
	active_fiber_(),
	rqueue_(),
	wqueue_(),
    f_idx_( wqueue_.get< f_tag_t >() ),
    tp_idx_( wqueue_.get< tp_tag_t >() )
{}

void
round_robin::spawn( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT( f);
    BOOST_ASSERT( ! f->is_terminated() );
    BOOST_ASSERT( f != active_fiber_);

    detail::fiber_base::ptr_t tmp = active_fiber_;
    BOOST_SCOPE_EXIT( & tmp, & active_fiber_) {
        active_fiber_ = tmp;
    } BOOST_SCOPE_EXIT_END
    active_fiber_ = f;
    RESUME_FIBER( active_fiber_);
}

void
round_robin::priority( detail::fiber_base::ptr_t const& f, int prio)
{
    BOOST_ASSERT( f);

    f->priority( prio);
}

void
round_robin::join( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT( f);
    BOOST_ASSERT( ! f->is_terminated() );
    BOOST_ASSERT( f != active_fiber_);

    if ( active_fiber_)
    {
        // store active-fiber as waiting fiber in p
        // detail::fiber_base::join() calls round_robin::wait()
        // so that active-fiber gets suspended
        f->join( active_fiber_);
        // suspend active-fiber until f terminates
        // fiber will be added to waiting-queue
        f_idx_.insert( schedulable( active_fiber_) );
        // set active_fiber to state_waiting
        active_fiber_->set_waiting();
        // suspend fiber
        active_fiber_->suspend();
        // fiber is resumed
        // f has teminated and active-fiber is resumed
    }
    else
    {
        while ( ! f->is_terminated() )
            run();
    }

    BOOST_ASSERT( f->is_terminated() );
}

void
round_robin::cancel( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT_MSG( false, "not implemented");
//  BOOST_ASSERT( f);
//  BOOST_ASSERT( f != active_fiber_);
//
//  // ignore completed fiber
//  if ( f->is_terminated() ) return;
//
//  detail::fiber_base::ptr_t tmp = active_fiber_;
//  {
//      BOOST_SCOPE_EXIT( & tmp, & active_fiber_) {
//          active_fiber_ = tmp;
//      } BOOST_SCOPE_EXIT_END
//      active_fiber_ = f;
//      // terminate fiber means unwinding its stack
//      // so it becomes complete and joining fibers
//      // will be notified
//      active_fiber_->terminate();
//  }
//  // erase completed fiber from waiting-queue
//  f_idx_.erase( f);
//
//  BOOST_ASSERT( f->is_terminated() );
}

bool
round_robin::run()
{
    // get all fibers with reached dead-line and push them
    // at the front of runnable-queue
    tp_idx_t::iterator e( tp_idx_.upper_bound( chrono::system_clock::now() ) );
    for (
            tp_idx_t::iterator i( tp_idx_.begin() );
            i != e; ++i)
    { rqueue_.push_front( i->f); }
    // remove all fibers with reached dead-line
    tp_idx_.erase( tp_idx_.begin(), e);

    // pop new fiber from runnable-queue which is not complete
    // (example: fiber in runnable-queue could be canceled by active-fiber)
    detail::fiber_base::ptr_t f;
    do
    {
        if ( rqueue_.empty() ) return false;
        f.swap( rqueue_.front() );
        rqueue_.pop_front();
        BOOST_ASSERT( f_idx_.end() == f_idx_.find( f) );
    }
    while ( f->is_terminated() ); //FIXME: test for state_terminated correct?
    detail::fiber_base::ptr_t tmp = active_fiber_;
    BOOST_SCOPE_EXIT( & tmp, & active_fiber_) {
        active_fiber_ = tmp;
    } BOOST_SCOPE_EXIT_END
    active_fiber_ = f;
    // resume new active fiber
    RESUME_FIBER( active_fiber_);
	return true;
}

void
round_robin::wait( detail::spin_mutex::scoped_lock & lk)
{
    BOOST_ASSERT( active_fiber_);
    BOOST_ASSERT( active_fiber_->is_running() );

    // fiber will be added to waiting-queue
    f_idx_.insert( schedulable( active_fiber_) );
    // set active_fiber to state_waiting
    active_fiber_->set_waiting();
    // unlock Lock assoc. with sync. primitive
    lk.unlock();
    // suspend fiber
    active_fiber_->suspend();
    // fiber is resumed

    BOOST_ASSERT( active_fiber_->is_running() );
}

void
round_robin::yield()
{
    BOOST_ASSERT( active_fiber_);
    BOOST_ASSERT( active_fiber_->is_running() );

    // yield() suspends the fiber and adds it
    // immediately to runnable-queue
    rqueue_.push_back( active_fiber_);
    // set active_fiber to state_ready
    active_fiber_->set_ready();
    // suspend fiber
    active_fiber_->yield();
    // fiber is resumed

    BOOST_ASSERT( active_fiber_->is_running() );
}

void
round_robin::sleep( chrono::system_clock::time_point const& abs_time)
{
    BOOST_ASSERT( active_fiber_);
    BOOST_ASSERT( active_fiber_->is_running() );

    if ( abs_time > chrono::system_clock::now() )
    {
        // fiber is added with a dead-line and gets suspended
        // each call of run() will check if dead-line has reached
        wqueue_.insert( schedulable( active_fiber_, abs_time) );
        // set active_fiber to state_waiting
        active_fiber_->set_waiting();
        // suspend fiber
        active_fiber_->suspend();
        // fiber is resumed, dead-line has been reached
    }

    BOOST_ASSERT( active_fiber_->is_running() );
}

void
round_robin::migrate_to( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT( f);
    BOOST_ASSERT( f->is_ready() );

    rqueue_.push_back( f);
}

detail::fiber_base::ptr_t
round_robin::migrate_from()
{
    detail::fiber_base::ptr_t f;

    if ( ! rqueue_.empty() )
    {
        f.swap( rqueue_.front() );
        rqueue_.pop_front();
        BOOST_ASSERT( f->is_ready() );
    }

    return f;
}

}}

#undef RESUME_FIBER

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

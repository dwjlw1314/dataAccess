/* boost random/bernoulli_distribution.hpp header file
 *
 * Copyright Jens Maurer 2000-2001
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org for most recent version including documentation.
 *
 * $Id$
 *
 * Revision history
 *  2001-02-18  moved to individual header files
 */

#ifndef BOOST_RANDOM_BERNOULLI_DISTRIBUTION_HPP
#define BOOST_RANDOM_BERNOULLI_DISTRIBUTION_HPP

#include <cassert>
#include <iostream>
#include <boost/random/detail/config.hpp>

namespace boost {

// Bernoulli distribution: p(true) = p, p(false) = 1-p   (boolean)
template<class RealType = double>
class bernoulli_distribution
{
public:
  // In principle, this could work with both integer and floating-point
  // types.  Generating floating-point random numbers in the first
  // place is probably more expensive, so use integer as input.
  typedef int input_type;
  typedef bool result_type;

  explicit bernoulli_distribution(const RealType& p_arg = RealType(0.5)) 
    : _p(p_arg)
  {
    assert(_p >= 0);
    assert(_p <= 1);
  }

  // compiler-generated copy ctor and assignment operator are fine

  RealType p() const { return _p; }
  void reset() { }

  template<class Engine>
  result_type operator()(Engine& eng)
  {
    if(_p == RealType(0))
      return false;
    else
      return RealType(eng() - (eng.min)()) <= _p * RealType((eng.max)()-(eng.min)());
  }

#ifndef BOOST_RANDOM_NO_STREAM_OPERATORS
  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const bernoulli_distribution& bd)
  {
    os << bd._p;
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, bernoulli_distribution& bd)
  {
    is >> std::ws >> bd._p;
    return is;
  }
#endif

private:
  RealType _p;
};

} // namespace boost

#endif // BOOST_RANDOM_BERNOULLI_DISTRIBUTION_HPP

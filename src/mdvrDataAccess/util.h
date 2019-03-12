/*
 * util.h
 *
 *  Created on: Aug 14, 2013
 *      Author: root
 */

#ifndef UTIL_H_
#define UTIL_H_

#include "includes.h"

namespace GsafetyAntDataAccess
{
	muduo::string split(muduo::string const& str, char ch, int n) throw(std::domain_error);
	muduo::string::const_iterator find_n_c(muduo::string const& str, char ch, int n);
	muduo::string::iterator find_n_c_noconst(muduo::string &str, char ch, int n);
}

#endif /* UTIL_H_ */

/*
 * util.cc
 *
 *  Created on: Aug 9, 2013
 *      Author: root
 */

/*
 * example::
 * 99dc0377,0108005606,wanlgl,C0,20151120 194536,V600,151120 194428,
 * 0,0,0,3,2,,,<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
 *
 * 20151120 19:45:36.540538Z 19299 INFO  90dc MDVR ID: 007202AC3B - mdvrServer.cc:155
 */

#include "mdvrServer.h"

using muduo::string;

namespace GsafetyAntDataAccess
{
	string split(string const& str, char ch, int n) throw(std::domain_error)
	{
		string::const_iterator i = str.begin();
		string::const_iterator j = std::find(i, str.end(), ch);
		int cnt = 1;

		while (j != str.end())
		{
			if (cnt != n)
			{
				i = std::find(i, str.end(), ch);
				//here have bug
				j = std::find(++i, str.end(), ch);
			}
			else
			{
				return string(i, j);
			}
			cnt += 1;
		}

		throw std::domain_error("Recv Parse Invalid msg.");
	}

	string::const_iterator find_n_c(string const& str, char ch, int n)
	{
		string::const_iterator i = str.begin();
		int cnt = 1;

		i = std::find(i, str.end(), ch);

		while (cnt != n)
		{
			if (i != str.end())
				i++;
			else
				return i;

			i = std::find(i, str.end(), ch);
			cnt += 1;
		}
		return i;
	}

	string::iterator find_n_c_noconst(muduo::string &str, char ch, int n)
	{
		string::iterator i = str.begin();
		int cnt = 1;

		i = std::find(i, str.end(), ch);

		while (cnt != n)
		{
			if (i != str.end())
				i++;
			else
				return i;

			i = std::find(i, str.end(), ch);
			cnt += 1;
		}
		return i;
	}
}


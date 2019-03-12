/*
 * vedioServer.cc
 *
 *  Created on: Aug 19, 2013
 *      Author: root
 */

#include "mdvrServer.h"

using namespace muduo;
using namespace muduo::net;

namespace GsafetyAntDataAccess
{
	//extern MutexLock mutex;
	void MdvrDataAccess::onVedioConnection(TcpConnectionPtr const& conn)
	{
		LOG_INFO << "VedioAccessServer - " << conn->peerAddress().toIpPort() << " -> "
			<< conn->localAddress().toIpPort() << " is " << (conn->connected() ? "UP" : "DOWN");

		if (conn->connected())
		{
			vedioConn_ = conn;
		}
		else
		{
			if (!conn->getContext3().empty())
			{
				string id_seq = boost::any_cast<string>(conn->getContext3());
				std::map<string, TcpConnectionPtr>:: iterator it = g_VedioConnectMap.find(id_seq);
				if (it == g_VedioConnectMap.end())
				{
					// not found;
					LOG_ERROR << "vedio " << id_seq << " not found in VedioConnectMap when offline.";
				}
				else
				{
					if (it->second == conn)
					{
						g_VedioConnectMap.erase(id_seq);
						LOG_INFO << "vedio connection map erase: " << id_seq;
						LOG_INFO << "VedioConnect Online Nums: " << g_VedioConnectMap.size();
					}
					else
					{
						LOG_INFO << "vedio connection map erase(dummy): " << id_seq;
					}
				}
			}
			else
			{
				LOG_INFO << "$$$$--unknown vedio connection is DOWN--$$$$";
			}
		}
	}

	//example:99dc0046,007201B39C,wanlgl,C65,150826 100000 100000,0#
	void MdvrDataAccess::onVedioMessage(TcpConnectionPtr const& conn, Buffer* buf, Timestamp time)
	{
		size_t length = buf->readableBytes();
		size_t bufIdx = 0;

		while (bufIdx < length)
		{
			if ((length - bufIdx) < 4)
			{
				LOG_INFO << "From VedioAccess: Data Is Too Short.";
				return;
			}
			LOG_INFO << "##########################################";
			string message;
			string header = buf->retrieveAsString(4);
			if (header == "99dc")
			{
				size_t end = buf->readableBytes();
				char const* st = buf->peek();
				char const* pos = std::find(st, st+end, '#');
				if (st+end != pos)
				{
					message = "99dc" + buf->retrieveAsString(pos - st + 1);

					LOG_INFO << "from VideoAccess: " << message;

					string const mdvrid = split(message, ',', 2);
					string const seqNum = split(message, ',', 3);

					std::map<string, TcpConnectionPtr>::iterator it = g_mdvrConnectMap.find(mdvrid);
					if (it != g_mdvrConnectMap.end())
					{
						string const reqCmd = split(message, ',', 4);
						if (reqCmd == "C39")
						{
							it->second->send(message);
							LOG_INFO << " transfer msg to mdvr: " << mdvrid;
							bufIdx += 4 + (pos - st + 1);
							continue;
						}

						std::map<string, TcpConnectionPtr>:: iterator iter = g_VedioConnectMap.find(mdvrid + seqNum);
						if (iter == g_VedioConnectMap.end())
						{
							// not found; add new mdvr
							g_VedioConnectMap[mdvrid + seqNum] = conn;
							conn->setContext3(mdvrid + seqNum);
							LOG_INFO << " transfer msg to mdvr: " << mdvrid;

							it->second->send(message);
						}
						else
						{
							if (conn != iter->second) {
								iter->second->shutdown();
								g_VedioConnectMap.erase(mdvrid + seqNum);
								g_VedioConnectMap[mdvrid + seqNum] = conn;
								conn->setContext3(mdvrid + seqNum);
								LOG_ERROR << "too many vedio cmd to a same mdvr";
							}
							else // receive some vedio cmd in the same conn
							{
								LOG_INFO << "receive some vedio cmd in the same connection";
								conn->send(message);
							}
						}
						LOG_INFO << "VedioConnect: " << g_VedioConnectMap.size();

					}
					else
					{
						/*
						 * don't send ack to vedio
						 */

						LOG_INFO << "mdvr: " << mdvrid << " not online.";
					}
					bufIdx = bufIdx + 4 + (pos - st + 1);
				}
				else //Back fill (a part of a command)
				{
					buf->prepend("99dc", 4);
					return ;
				}
			}
			else
			{
				// if the header of the pocket not 99dc, discard the entire pocket
				buf->retrieveAll();
				LOG_INFO << "from VideoAccess: Invalid pocket.";
				return ;
			}
		}
	}
}

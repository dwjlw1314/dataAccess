/*
 * GpsFileServer.cc
 *
 *  Created on: may 28, 2016
 *      Author: wanlgl
 */

#include "mdvrServer.h"

using namespace muduo;
using namespace muduo::net;

namespace GsafetyAntDataAccess
{
	std::map<string, TcpConnectionPtr> g_GpsFileConnectMap;

	void MdvrDataAccess::onGpsFileConnection(const muduo::net::TcpConnectionPtr& conn)
	{
		LOG_INFO << "GPSFileDownServer --- " << conn->peerAddress().toIpPort() << " -> "
		   << conn->localAddress().toIpPort() << " is " << (conn->connected() ? "UP" : "DOWN");
		if (!conn->connected())
		{
			if (!conn->getContext().empty())
			{
				string mdvrid = boost::any_cast<string>(conn->getContext());
				std::map<string, TcpConnectionPtr>::iterator it = g_GpsFileConnectMap.find(mdvrid);
				if (it == g_GpsFileConnectMap.end())
				{
					//not found;
					LOG_INFO << "MDVR:" << mdvrid << "--not found GPS file downing!!";
				}
				else
				{
					g_GpsFileConnectMap.erase(mdvrid);
					sendGpsFileOverCmd2MQ(mdvrid);
					LOG_INFO << "MDVR:" << mdvrid << "--GPS File Downing Finished!!";
				}
			}
		}
	}

	void MdvrDataAccess::onGpsFileMessage(const muduo::net::TcpConnectionPtr& conn,
			muduo::net::Buffer* buf, muduo::Timestamp time)
	{
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
				string cmd = split(message, ',', 4);
				cmdGPSFileDownProxy2MQ(message, cmd, conn);
			}
			else
			{
				//Back fill (a part of a command)
				buf->prepend("99dc", 4);
				return;
			}
		}
		else
		{
			string mdvrid = boost::any_cast<string>(conn->getContext());
			std::map<string, TcpConnectionPtr>::iterator it = g_GpsFileConnectMap.find(mdvrid);

			if (it != g_GpsFileConnectMap.end())
			{
				muduo::string key = "MDVR.V4." + mdvrid;
				string msg = header + buf->retrieveAllAsString();
				send2MQ(ex_, msg, key);
			}
			else
			{
				buf->retrieveAll();
				LOG_INFO << "MDVR:" << mdvrid << "--Invalid command(pocket).";
			}
		}
	}

	//V4
	void MdvrDataAccess::cmdGPSFileDownProxy2MQ(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		string const mdvrid = split(msg, ',', 2);
		std::map<string, TcpConnectionPtr>::iterator it = g_GpsFileConnectMap.find(mdvrid);
		if (it == g_GpsFileConnectMap.end())
		{
			//not found;
			g_GpsFileConnectMap[mdvrid] = conn; //new MDVR GPS file downing
			conn->setContext(mdvrid);
			LOG_INFO << "GPS File Downing Register :" << mdvrid;
			muduo::string key = "MDVR." + cmd + "." + mdvrid;
			send2MQ(ex_, msg, key);
		}
		else
		{
			if (conn != it->second)
			{
				it->second->shutdown();
				g_GpsFileConnectMap.erase(mdvrid);
				g_GpsFileConnectMap[mdvrid] = conn;
				conn->setContext(mdvrid);
				muduo::string key = "MDVR." + cmd + "." + mdvrid;
				LOG_INFO << "@GPS File Downing replace: " << mdvrid;
				send2MQ(ex_, msg, key);
			}
			else
			{
				//same conn repeat V1
				LOG_INFO << "@GPS File Downing repeat: " << mdvrid;
				muduo::string key = "MDVR." + cmd + "." + mdvrid;
				send2MQ(ex_, msg, key);
			}
		}

		//create ack to mdvr
		string ack;
		Timestamp time;

		string::const_iterator ib = msg.begin();
		ack =  string(ib, find_n_c(msg, ',', 3)) + ",C0," + time.now().toAntFormattedString()
				+ "," + cmd + "," + split(msg, ',', 5) + CMD_RESP_CONTENT_SORF;

		string::size_type len = ack.size();
		len -= 8;

		char temp[5];
		sprintf(temp, "%04d", len);
		std::copy(temp, temp + 4, ack.begin() + 4);

		conn->send(ack); //ack to mdvr

		LOG_INFO << "To MDVR ACK: " << ack;
	}

	void MdvrDataAccess::sendGpsFileOverCmd2MQ(muduo::string const& mdvrid)
	{
		char temp[5];
		Timestamp time;
		string finishCmd;
		finishCmd = "99dc0032," + mdvrid + ",,A4," + time.now().toAntFormattedString() + "#";
		string key = "MDVR.V4."+ mdvrid;

		string::size_type len = finishCmd.size();
		sprintf(temp, "%04d", len-8);
		std::copy(temp, temp + 4, finishCmd.begin() + 4);

		try
		{
			ex_->Publish(finishCmd , key); // send to MQ
			LOG_INFO << "To MQ: " << finishCmd;
		}catch (AMQPException e)
		{
			LOG_INFO << e.getMessage();
		}
	}
}

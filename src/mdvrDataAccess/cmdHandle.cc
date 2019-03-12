/*
 * mdvraccess.cc
 *
 *  Created on: Aug 9, 2013
 *      Author: root
 */

#include "AMQP/AMQPcpp.h"
#include "mdvrServer.h"
#include <cstdio>

using namespace muduo;
using namespace muduo::net;

namespace GsafetyAntDataAccess
{
	//MutexLock mutex;
	EventLoop* g_loop;
	std::string g_MQConfig;
	MdvrDataAccess* g_server;
	MutexLock g_vedioConnMapMutex;

	// global variable
	std::map<string, TcpConnectionPtr> g_mdvrConnectMap;
	std::map<string, TcpConnectionPtr> g_VedioConnectMap;

	//V1
	void MdvrDataAccess::cmdRegister(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		string const mdvrid = split(msg, ',', 2);
		std::map<string, TcpConnectionPtr>::iterator it = g_mdvrConnectMap.find(mdvrid);
		if (it == g_mdvrConnectMap.end())
		{
			//not found;
			g_mdvrConnectMap[mdvrid] = conn; //new mdvr online
			conn->setContext(mdvrid);
			LOG_INFO << "@mdvr online (new): " << mdvrid;
			muduo::string key = "MDVR." + cmd + "." + mdvrid;
			send2MQ(ex_, msg, key);
		}
		else
		{
			if (conn != it->second)
			{
				it->second->shutdown();
				g_mdvrConnectMap.erase(mdvrid);
				g_mdvrConnectMap[mdvrid] = conn;
				conn->setContext(mdvrid);
				LOG_INFO << "@mdvr online (replace): " << mdvrid;
			}
			else
			{
				//same conn repeat V1
				LOG_INFO << "@mdvr online (repeat): " << mdvrid;
				muduo::string key = "MDVR." + cmd + "." + mdvrid;
				send2MQ(ex_, msg, key);
			}
		}
		LOG_INFO << "@MDVR Online Nums: " << g_mdvrConnectMap.size();

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

	//V20
	void MdvrDataAccess::cmdResponseEmptyProxy_No2MQ(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		string ack;
		Timestamp time;

		string::const_iterator ib = msg.begin();

		ack = string(ib, find_n_c(msg, ',', 3)) + ",C0," + time.now().toAntFormattedString()
				+ "," + cmd + "," + split(msg, ',', 5) + CMD_RESP_CONTENT_EMPTY;

		string::size_type len = ack.size();
		len -= 8;

		char temp[5];

		sprintf(temp, "%04d", len);
		std::copy(temp, temp + 4, ack.begin() + 4);

		conn->send(ack); //ack to mdvr

		LOG_INFO << "To MDVR ACK: " << ack;
		muduo::string key = "MDVR." + cmd + "." + split(msg, ',', 2);
		send2MQ(ex_, msg, key);
	}

	//V77
	void MdvrDataAccess::cmdResponseEmptyProxy(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		string ack;
		Timestamp time;

		string::const_iterator ib = msg.begin();

		ack = string(ib, find_n_c(msg, ',', 3)) + ",C0," + time.now().toAntFormattedString()
				+ "," + cmd + "," + split(msg, ',', 5) + CMD_RESP_CONTENT_EMPTY;

		string::size_type len = ack.size();
		len -= 8;

		char temp[5];

		sprintf(temp, "%04d", len);
		std::copy(temp, temp + 4, ack.begin() + 4);

		conn->send(ack); //ack to mdvr

		LOG_INFO << "To MDVR ACK: " << ack;

		muduo::string key = "MDVR." + cmd + "." + split(msg, ',', 2);

		send2MQ(ex_, msg, key);
	}

	//V600
	void MdvrDataAccess::cmdXmlTransfer(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		string tmp, ack;
		Timestamp time;
		string ackContent("<\?xml version=\"1.0\" encoding=\"UTF-8\" "
			"standalone=\"yes\" \?>\r\n"
			"<MDVRBus version=\"120825\">\r\n"
			"<CheckInfo UNIT_ID=\"1234567890\" _TIME_=\"2012/04/01 16:10:43 SUN\" _ZONE_=\"+8\">\r\n"
			"<InfoClass _TYPE_=\"InspectInfo\">\r\n"
			"<InfoList>\r\n"
			"<_INFO_ _NAME_=\"InspectResult\" _STAT_=\"pass\" />\r\n"
			"</InfoList>\r\n"
			"</InfoClass>\r\n"
			"</CheckInfo>\r\n"
			"</MDVRBus>");

		//verification
		string::iterator pos = find(msg.begin(), msg.end(), '<');
		if (pos != msg.end())
		{
			//found
			if (count(msg.begin(), pos, ',') != 24)
			{
				LOG_INFO << "invalid V600 cmd.";
				return;
			}
		}
		else
		{
			LOG_INFO << "invalid V600 cmd: no '<'.";
			return;
		}

		string::const_iterator ib = msg.begin();

		tmp = string(ib, find_n_c(msg, ',', 3)) + ",C0," + time.now().toAntFormattedString()
				+ "," + cmd + ","
				+ split(msg, ',', 5)  //src time
				+ ",0,0,0," //ack patt, succ flag, ack style
				+ split(msg, ',', 21) + ","//msg type
				+ split(msg, ',', 22) + ","//session id
				;

		string const msgType = split(msg, ',', 21); //msg_type

		switch (*(find_n_c(msg, ',', 20) + 1))
		{
			// msg type
			case '1':
				LOG_INFO << "V600 msg type = 1";
				//to do
				return ;
			case '2':
				LOG_INFO << "V600 msg type = 2";
				//to do
				return ;
			case '3':
				ack = tmp
					+ ",," //len, len
					+ ackContent
					+ ",,#"; //error code, fail reason
				break;
			case '4':
				ack = tmp + ",,,,,#";
				break;
			case '5':
				ack = tmp + ",,,,,#";
				break;
			case '6':
				ack = tmp + ",,,,,#";
				break;
			case '7':
				ack = tmp + ",,,,,#";
				break;
			default:
				LOG_INFO << "Invalid msg type of V600 cmd";
				return ;
		}

		string::size_type len = ack.size();
		len -= 8;

		char temp[5];

		sprintf(temp, "%04d", len);
		std::copy(temp, temp + 4, ack.begin() + 4);

		conn->send(ack); //ack to mdvr

		LOG_INFO << "To MDVR ACK: " << ack;

		muduo::string key = "MDVR." + cmd + "." + split(msg, ',', 2);
		send2MQ(ex_, msg, key);
	}

	//V0
	void MdvrDataAccess::cmdRespUser(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		string const reqCmd = split(msg, ',', 21);
		if ((reqCmd == "C39") || (reqCmd == "C110") || (reqCmd == "C114"))
		{
			if (vedioConn_) //如果图像端在线
				vedioConn_->send(msg); //send to vedio server
			else
				LOG_INFO << "vedio connection is DOWN!";
		}
		else
		{
			string key = "MDVR." + cmd + reqCmd + "." + split(msg, ',', 2);
			try
			{
				send2MQ(ex_, msg, key); //send to MQ
			}catch (AMQPException e)
			{
				LOG_INFO << e.getMessage();
			}
		}
	}

	//V21 V22 V23 (include GPS file down type)
	void MdvrDataAccess::cmdNoRespProxy2VedioServer(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		if (vedioConn_) //如果图像端在线
		{
			LOG_INFO << "vedio connection online currently!";
			vedioConn_->send(msg);
		}
		else
		{
			LOG_INFO << "vedio connection not online currently!";
		}
	}

	//V30
	void MdvrDataAccess::cmdGPSNoRespProxy2MQ(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		muduo::string key = "MDVR." + cmd + "." + split(msg, ',', 2);
		send3MQ(exGps_, msg, key);
	}

	//V39
	void MdvrDataAccess::cmdNoRespProxy2MQ(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		muduo::string key = "MDVR." + cmd + "." + split(msg, ',', 2);
		send2MQ(ex_, msg, key);
	}

	//V61 V63 V64 V68 V69 V70 V75 V78 V79 V83
	void MdvrDataAccess::cmdAlertProxy(string& msg, string const& cmd, TcpConnectionPtr const& conn)
	{
		string ack;

		Timestamp time;

		string::const_iterator ib = msg.begin();

		//警情id在第36和37个逗号之间
		ack = string(ib, find_n_c(msg, ',', 3)) + ",C0," + time.now().toAntFormattedString()
				+ "," + cmd + "," + split(msg, ',', 5) + ",0," + split(msg, ',', 37) + "#";

		string::size_type len = ack.size();
		len -= 8;

		char temp[5];

		sprintf(temp, "%04d", len);
		std::copy(temp, temp + 4, ack.begin() + 4);

		conn->send(ack); //ack to mdvr

		LOG_INFO << "To MDVR ACK: " << ack;

		string key = "MDVR." + cmd + "." + split(msg, ',', 2);
		try
		{
			send2MQ(ex_, msg, key); //send to MQ
		} catch (AMQPException e)
		{
			LOG_INFO << e.getMessage();
		}
	}

	void MdvrDataAccess::callMdvrcmdHandler(string& cmd, string& msg, TcpConnectionPtr const& conn)
	{
		if (mdvrCmdMap.find(cmd) == mdvrCmdMap.end())
			throw std::domain_error(msg + "###Handle Invalid cmd.");
		else
			(this->*mdvrCmdMap[cmd])(msg, cmd, conn);
	}
}

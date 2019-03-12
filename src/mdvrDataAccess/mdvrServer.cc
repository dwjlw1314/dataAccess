#include "mdvrServer.h"
#include <cstdio>

using namespace muduo;
using namespace muduo::net;

//心跳90dc00xx

namespace GsafetyAntDataAccess
{
	//extern MutexLock mutex;
	MdvrDataAccess::MdvrDataAccess(EventLoop* loop, InetAddress const& listenMdvrAddr,
			InetAddress const& listenVedioAddr, InetAddress const& listenGpsAddr,
			InetAddress const& listenGpsFileAddr, int idleSeconds)
	  : loop_(loop), amqp_(g_MQConfig), amqpGps_(g_MQConfig),
		server_(loop, listenMdvrAddr, "mdvrAccessServer"),
		serverVedio_(loop, listenVedioAddr, "vedioAccessServer"),
		serverGps_(loop, listenGpsAddr, "gpsAccessServer"),
		serverGpsFile_(loop, listenGpsFileAddr, "gpsFileDownServer"),
		connectionBuckets_(idleSeconds)
	{
		server_.setConnectionCallback(
			boost::bind(&MdvrDataAccess::onMdvrConnection, this, _1));
		server_.setMessageCallback(
			boost::bind(&MdvrDataAccess::onMdvrMessage, this, _1, _2, _3));

		loop->runEvery(1.0, boost::bind(&MdvrDataAccess::onTimer, this));
		connectionBuckets_.resize(idleSeconds); //开启后会导致程序崩溃

		serverVedio_.setConnectionCallback(
			boost::bind(&MdvrDataAccess::onVedioConnection, this, _1));
		serverVedio_.setMessageCallback(
			boost::bind(&MdvrDataAccess::onVedioMessage, this, _1, _2, _3));

		/*
		 * GPS connect not use connection timeout destruction mechanism
		 */
		serverGps_.setConnectionCallback(
			boost::bind(&MdvrDataAccess::onGpsConnection, this, _1));
		serverGps_.setMessageCallback(
			boost::bind(&MdvrDataAccess::onGpsMessage, this, _1, _2, _3));

		/*
		 * GPS file downing connect not use connection timeout destruction mechanism
		 */
		serverGpsFile_.setConnectionCallback(
			boost::bind(&MdvrDataAccess::onGpsFileConnection, this, _1));
		serverGpsFile_.setMessageCallback(
			boost::bind(&MdvrDataAccess::onGpsFileMessage, this, _1, _2, _3));

		//信令通道注册指令
		mdvrCmdMap[CMD_REGISTER] = &MdvrDataAccess::cmdRegister;
#ifdef POWEROFF
		//mdvrCmdMap[CMD_POWEROFF] = &MdvrDataAccess::cmdResponseEmptyProxy;
		mdvrCmdMap[CMD_POWEROFF] = &MdvrDataAccess::cmdResponseEmptyProxy_No2MQ;
#endif
		//安全套件警情解除通知
		mdvrCmdMap[CMD_ALERT_REMOVE] = &MdvrDataAccess::cmdResponseEmptyProxy;

		mdvrCmdMap[CMD_XML] = &MdvrDataAccess::cmdXmlTransfer;

		//安全套件应答交通监控中心下发的指令
		mdvrCmdMap[CMD_RESP_USER] = &MdvrDataAccess::cmdRespUser;

		/* VideoAccess to return the result */
		mdvrCmdMap[CMD_VEDIO_LINK_CREATE_FAIL] = &MdvrDataAccess::cmdNoRespProxy2VedioServer;
		mdvrCmdMap[CMD_VEDIO_SESSION_START] = &MdvrDataAccess::cmdNoRespProxy2VedioServer;
		mdvrCmdMap[CMD_VEDIO_SESSION_END] = &MdvrDataAccess::cmdNoRespProxy2VedioServer;

		//上报当前位置
		mdvrCmdMap[CMD_GPS] = &MdvrDataAccess::cmdGPSNoRespProxy2MQ;
		//安全套件状态信息上传
		mdvrCmdMap[CMD_MDVR_STATE] = &MdvrDataAccess::cmdNoRespProxy2MQ;
		//一键报警
		mdvrCmdMap[CMD_ONEKEY_ALARM] = &MdvrDataAccess::cmdAlertProxy;
		//温度报警
		mdvrCmdMap[CMD_TEMP_ALARM] = &MdvrDataAccess::cmdAlertProxy;
		//MDVR存储器错误报警
		mdvrCmdMap[CMD_SDCARD_ERROR_ALARM] = &MdvrDataAccess::cmdAlertProxy;
		//摄像头无信号告警 SIMA ADD(厄瓜二期新增)
		mdvrCmdMap[CMD_CAMERA_NOSIGNAL_ALARM] = &MdvrDataAccess::cmdAlertProxy;
		//摄像头遮挡报警,由于遮挡报警测量不准确，要完全遮挡了才能呈现，实际用途不大，所以不使用这种报警
		mdvrCmdMap[CMD_CAMERA_SHIELD_ALARM] = &MdvrDataAccess::cmdAlertProxy;
		//超速报警
		mdvrCmdMap[CMD_OVER_SPEED_ALERT] = &MdvrDataAccess::cmdAlertProxy;
		//GPS接收机故障报警
		mdvrCmdMap[CMD_GPS_RECEIVER_ALERT] = &MdvrDataAccess::cmdAlertProxy;
		//电压异常报警
		mdvrCmdMap[CMD_VOLTAGE_ALERT] = &MdvrDataAccess::cmdAlertProxy;
		//区域报警
		mdvrCmdMap[CMD_AREA_ALERT] = &MdvrDataAccess::cmdAlertProxy;
#ifdef FIREALERT
		//点火报警
		mdvrCmdMap[CMD_FIRE_ALERT] = &MdvrDataAccess::cmdAlertProxy;
#endif
		//异常开关门报警
		mdvrCmdMap[CMD_DOOR_ALERT] = &MdvrDataAccess::cmdAlertProxy;

		try
		{
			ex_ = amqp_.createExchange("MDVR_EXCHANGE");
			ex_->Declare("MDVR_EXCHANGE", "topic", 2);

			ex_->setHeader("Delivery-mode", 2);
			ex_->setHeader("Content-type", "text/text");
			ex_->setHeader("Content-encoding", "UTF-8");

			exGps_ = amqpGps_.createExchange("GPS_EXCHANGE");
			exGps_->Declare("GPS_EXCHANGE", "topic", 2);

			exGps_->setHeader("Delivery-mode", 2);
			exGps_->setHeader("Content-type", "text/text");
			exGps_->setHeader("Content-encoding", "UTF-8");
		}catch (AMQPException e)
		{
			throw e;
		}
	}

	void MdvrDataAccess::onTimer()
	{
		connectionBuckets_.push_back(Bucket());
	}

	void MdvrDataAccess::start()
	{
		server_.start();
		serverVedio_.start();
		serverGps_.start();
		serverGpsFile_.start();
	}

	void MdvrDataAccess::onMdvrConnection(TcpConnectionPtr const& conn)
	{
		LOG_INFO << "MDVRDataAccessServer --- " << conn->peerAddress().toIpPort() << " -> "
		   << conn->localAddress().toIpPort() << " is " << (conn->connected() ? "UP" : "DOWN");
		if (conn->connected())
		{
			EntryPtr entry(new Entry(conn));
			connectionBuckets_.back().insert(entry);
			WeakEntryPtr weakEntry(entry);
			conn->setContext2(weakEntry);
		}
		else
		{
		    //assert(!conn->getContext2().empty());
			if (!conn->getContext().empty())
			{
				string mdvrid = boost::any_cast<string>(conn->getContext());
				//MutexLockGuard lock(mutex);
				std::map<string, TcpConnectionPtr>::iterator it = g_mdvrConnectMap.find(mdvrid);
				if (it == g_mdvrConnectMap.end())
				{
					//not found;
					LOG_INFO << "mdvr: " << mdvrid << "--not found in mdvrConnectMap when offline.";
				}
				else
				{
					if (it->second == conn)
					{
						g_mdvrConnectMap.erase(mdvrid);
						LOG_INFO << "@mdvr offline : " << mdvrid;
						LOG_INFO << "@MDVR online Nums: " << g_mdvrConnectMap.size();
						sendOfflineCmd2MQ(mdvrid);
					}
					else
					{
						LOG_INFO << "mdvr offline(dummy): " << mdvrid;
					}
				}
			}
			else
			{
				LOG_INFO << "####--unknown mdvr connection is DOWN--#####";
			}
		}
	}

	void MdvrDataAccess::onMdvrMessage(TcpConnectionPtr const& conn, Buffer* buf, Timestamp time)
	{
		assert(!conn->getContext2().empty());
		WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext2()));
		EntryPtr entry(weakEntry.lock());
		if (entry)
		{
			connectionBuckets_.back().insert(entry);
		}

		size_t bufIdx = 0;
		size_t length = buf->readableBytes();

		while (bufIdx < length)
		{
			if ((length - bufIdx) < 4)
			{
				LOG_INFO << "From MDVR: Data Is Too Short.";
				return;
			}

			string message;
			string header = buf->retrieveAsString(4);
			if (header == "90dc") //心跳
			{
				bufIdx += 4;
				if (!conn->getContext().empty())
				{
					LOG_INFO << "90dc MDVR ID: " << boost::any_cast<string>(conn->getContext());
				}
				else
				{
					LOG_WARN << "90dc MDVR ID: NULL";
				}
				conn->send("90dc");
			}
			else if (header == "99dc")
			{
				size_t end = buf->readableBytes();
				char const* st = buf->peek();
				char const* pos = std::find(st, st+end, '#');
				if (st+end != pos)
				{
					message = "99dc" + buf->retrieveAsString(pos - st + 1);

					LOG_INFO << message; //Displays full instructions

					try
					{
						string cmd = split(message, ',', 4);
						callMdvrcmdHandler(cmd, message, conn);
					} catch(std::domain_error e)
					{
						LOG_INFO << e.what();
						break;
					}
					bufIdx = bufIdx + 4 + (pos - st + 1);
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
				// if the header of the pocket not 90dc or 99dc, discard the entire pocket
				buf->retrieveAll();
				LOG_INFO << "From MDVR: Invalid command(pocket).";
				return;
			}
		}
	}

	void MdvrDataAccess::sendOfflineCmd2MQ(muduo::string const& mdvrid)
	{
		char temp[5];
		Timestamp time;
		string offlineCmd;
		offlineCmd = "99dc0032," + mdvrid + ",,A1," + time.now().toAntFormattedString() + "#";
		string key = "MDVR.A1."+ mdvrid;

		string::size_type len = offlineCmd.size();
		sprintf(temp, "%04d", len-8);
		std::copy(temp, temp + 4, offlineCmd.begin() + 4);

		try
		{
			ex_->Publish(offlineCmd , key); // send to MQ
			LOG_INFO << "To MQ: " << offlineCmd;
		}catch (AMQPException e)
		{
			LOG_INFO << e.getMessage();
		}
	}
}

/*
 * rcvMQ.cc
 *
 *  Created on: Aug 17, 2013
 *      Author: root
 */

#include "util.h"
#include "mdvrServer.h"

#include <muduo/base/ProcessInfo.h>

#include <netdb.h>
#include <arpa/inet.h>

using namespace muduo;
using namespace muduo::net;

namespace GsafetyAntDataAccess
{
	//extern MutexLock mutex;
	extern EventLoop* g_loop;

	void findMdvrConnectInLoop(string msg)
	{
		g_loop->assertInLoopThread();
		string const mdvrid = split(msg, ',', 2);
		std::map<string, TcpConnectionPtr>:: iterator it = g_mdvrConnectMap.find(mdvrid);
		if (it != g_mdvrConnectMap.end())
		{
			it->second->send(msg);
			LOG_INFO << "to mdvr: "<< msg;
		}
		else
		{
			// not found;
			LOG_INFO << " mdvr : " << mdvrid << " not online.";
			/*
			 *  TODO: ack this information to MQ
			 *  The reply of how to implement
			 */
		}
	}

	static int onCancel(AMQPMessage * message)
	{
		std::cout << "cancel tag="<< message->getDeliveryTag() << std::endl;
		return 0;
	}

	static int  onMessage(AMQPMessage * message)
	{
		uint32_t j = 0;

		char * data = message->getMessage(&j);

		if (data)
		{
			LOG_INFO << "from MQ: "<< data;
		}

		string msg(data);
		string::size_type pos = msg.find("99dc");
		if (pos == 0) //found 99dc
		{
			string::iterator it = std::find(msg.begin(), msg.end(), '#');
			if (it != msg.end()) //found #
			{
				msg.erase(it+1, msg.end());
			}
			else //not found #
			{
				LOG_INFO << "not found # in cmd received from MQ, discard.";
				return 0;
			}
		}
		else
		{
			LOG_INFO << "invaild cmd header received from MQ, discard.";
			return 0;
		}

		g_loop->queueInLoop(boost::bind(&findMdvrConnectInLoop, msg));

		return 0;
	}

	void MdvrDataAccess::threadRcvMQ()
	{
		printf("RcvMQThread pid = %d, tid = %d\n", ::getpid(), muduo::CurrentThread::tid());
		try
		{
			AMQP amqp(g_MQConfig); //all connect string
			AMQPQueue * qu = amqp.createQueue("MdvrQueue");
			qu->Declare();

			qu->Bind( "MDVR_EXCHANGE", "MDVR.C3.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C4.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C30.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C31.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C32.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C33.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C39.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C43.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C57.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C58.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C62.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C64.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C65.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C68.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C69.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C70.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C78.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C79.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C80.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C81.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C82.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C83.*");

			qu->Bind( "MDVR_EXCHANGE", "MDVR.C101.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C107.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C110.*");
			//搜索录像文件(带会话ID)走图像服务
			//qu->Bind( "MDVR_EXCHANGE", "MDVR.C111.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C114.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C115.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C129.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C130.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C150.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C152.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C154.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C158.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C162.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C163.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C164.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C165.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C166.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C167.*");

			qu->Bind( "MDVR_EXCHANGE", "MDVR.C601.*");
			qu->Bind( "MDVR_EXCHANGE", "MDVR.C602.*");


			qu->addEvent(AMQP_MESSAGE, onMessage);
			qu->addEvent(AMQP_CANCEL, onCancel);

			qu->Consume(AMQP_NOACK);
		}  catch (AMQPException e)
		{
			LOG_INFO << e.getMessage();
			printf("ERROR: RcvMQ thread end.\n");
		}
	}

#ifdef SATCOMGPS
	void MdvrDataAccess::threadRaptorServer()
	{
		printf("RaptorRcvMQThread pid = %d, tid = %d\n", ::getpid(), muduo::CurrentThread::tid());
		//no used
	}
	RaptorRcv.start();
#endif
#ifdef SATCOMGPS
	void MdvrDataAccess::threadSatcomServer()
	{
		printf("SatcomRcvMQThread pid = %d, tid = %d\n", ::getpid(), muduo::CurrentThread::tid());
		//no used;
	}
#endif
}

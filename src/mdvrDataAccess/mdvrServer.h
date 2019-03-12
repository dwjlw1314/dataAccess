#ifndef MDVRSERVER_H_
#define MDVRSERVER_H_

#include <string>
#include "includes.h"
#include "AMQP/AMQPcpp.h"
#include "cmdHandle.h"
#include "GpsServer.h"

/* v4.0 cancel v82/v20 */
/* Function of the switch */
/*
 * #define POWEROFF
 * #define FIREALERT
 */

namespace GsafetyAntDataAccess
{
//NameSpace Area
extern std::string g_MQConfig;
extern std::string g_MQGpsPrefix;
extern std::map<muduo::string, muduo::net::TcpConnectionPtr> g_mdvrConnectMap;
extern std::map<muduo::string, muduo::net::TcpConnectionPtr> g_VedioConnectMap;
extern std::map<muduo::string, muduo::net::TcpConnectionPtr> g_GpsConnectMap;

class MdvrDataAccess : private boost::noncopyable
{
	private:
		muduo::net::EventLoop* loop_;
		muduo::net::TcpServer server_;
		muduo::net::TcpServer serverVedio_;
		muduo::net::TcpServer serverGps_;
		muduo::net::TcpServer serverGpsFile_;

		typedef void (MdvrDataAccess::*mdvrCmdHandler) (muduo::string& msg,
					muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		std::map<muduo::string, mdvrCmdHandler> mdvrCmdMap;

		void cmdRegister(muduo::string& msg,muduo::string const& cmd,muduo::net::TcpConnectionPtr const& conn);
		void cmdResponseEmptyProxy(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdXmlTransfer(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdRespUser(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdNoRespProxy2VedioServer(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdNoRespProxy2MQ(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdGPSNoRespProxy2MQ(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdAlertProxy(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdResponseEmptyProxy_No2MQ(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);
		void cmdGPSFileDownProxy2MQ(muduo::string& msg, muduo::string const& cmd, muduo::net::TcpConnectionPtr const& conn);

		void callMdvrcmdHandler(muduo::string& cmd, muduo::string& msg, muduo::net::TcpConnectionPtr const& conn);
		inline void send2MQ(AMQPExchange* ex, muduo::string const& msg, muduo::string const& key) const
		{
			try
			{
				//send to MQ
				ex->Publish(msg, key);
				LOG_INFO << "Published to MQ: "<< msg;
			} catch (AMQPException& e) {
				LOG_INFO << e.getMessage();
				exit(0);
			}
		}

		//gps data  include v30/$GPRMC
		inline void send3MQ(AMQPExchange* ex, muduo::string const& msg, muduo::string const& key) const
		{
			try
			{
				ex->Publish(msg, key);
				LOG_INFO << "GPS Data To MQ:" << msg;
			} catch (AMQPException& e) {
				LOG_INFO << e.getMessage();
				exit(0);
			}
		}

		/*
		 * RabbitMQ About Information
		 */
		AMQP amqp_;
		AMQPExchange* ex_;
		void onMdvrConnection(const muduo::net::TcpConnectionPtr& conn);
		void onMdvrMessage(const muduo::net::TcpConnectionPtr& conn,
				muduo::net::Buffer* buf, muduo::Timestamp time);

		/*
		 * VideoAccess about
		 */
		AMQP amqpGps_;
		AMQPExchange* exGps_;
		void onVedioConnection(const muduo::net::TcpConnectionPtr& conn);
		void onVedioMessage(const muduo::net::TcpConnectionPtr& conn,
				muduo::net::Buffer* buf, muduo::Timestamp time);
		muduo::net::TcpConnectionPtr vedioConn_;

		/*
		 * GPS Connecting Business processing function
		 */
		void onGpsConnection(const muduo::net::TcpConnectionPtr& conn);
		void onGpsMessage(const muduo::net::TcpConnectionPtr& conn,
				muduo::net::Buffer* buf, muduo::Timestamp time);
		void gpsDataAnalysis(std::string const& gpsData, size_t length);

		/*
		 * GPS File Downing callback function
		 */
		void onGpsFileConnection(const muduo::net::TcpConnectionPtr& conn);
		void onGpsFileMessage(const muduo::net::TcpConnectionPtr& conn,
				muduo::net::Buffer* buf, muduo::Timestamp time);

		/*
		 * connection timeout
		 */
		void onTimer();

		typedef boost::weak_ptr<muduo::net::TcpConnection> WeakTcpConnectionPtr;

		struct Entry : public muduo::copyable
		{
			explicit Entry(const WeakTcpConnectionPtr& weakConn)
			  : weakConn_(weakConn)
			{ /* empty */ }

			~Entry()
			{
				muduo::net::TcpConnectionPtr conn = weakConn_.lock();
				if (conn)
				{
					if (!conn->getContext().empty())
					{
						std::string mdvrid = boost::any_cast<std::string>(conn->getContext());
						LOG_INFO << "Connection Timeout:mdvr(" << mdvrid << ")";
					}
					else
					{
						LOG_WARN<< "mdvr id NULL";
					}
					//loopback heartbeat
					conn->send("90dc");
				}
			}

			WeakTcpConnectionPtr weakConn_;
		};

		typedef boost::shared_ptr<Entry> EntryPtr; //共享型Entry指针
		typedef boost::weak_ptr<Entry> WeakEntryPtr; //弱指针Entry型
		typedef boost::unordered_set<EntryPtr> Bucket; //共享型Entry集合
		typedef boost::circular_buffer<Bucket> WeakConnectionList;

		WeakConnectionList connectionBuckets_;

	public:
		MdvrDataAccess(muduo::net::EventLoop* loop,
			const muduo::net::InetAddress& listenMdvrAddr,
			const muduo::net::InetAddress& listenVedioAddr,
			const muduo::net::InetAddress& listenGpsAddr,
			const muduo::net::InetAddress& listenGpsFileAddr,
			int idleSeconds);
		void start();
		void threadRcvMQ();
		void sendOfflineCmd2MQ(muduo::string const& mdvrid);
		void sendGpsFileOverCmd2MQ(muduo::string const& mdvrid);

#ifdef RAPTORGPS
		void threadRaptorServer();
#endif
#ifdef SATCOMGPS
		void threadSatcomServer();
#endif
	};

	/*
	 * global used
	 */
	extern MdvrDataAccess* g_server;
}
#endif  // MDVRSERVER_H_

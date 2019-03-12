#include <muduo/base/Thread.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/AsyncLogging.h>
#include <muduo/base/Timestamp.h>

#include <sys/resource.h>
#include <unistd.h> //daemon()
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstdio>
#include <errno.h>

#include <boost/bind.hpp>

#include "libconfig.h++"
#include "mdvrServer.h"

using namespace GsafetyAntDataAccess;
using namespace muduo::net;
using namespace libconfig;
using namespace std;

#define KONEGB 1000*1024*1024
#define KROLLSIZE 500*1024*1024
#define LOGNAMELENGTH 256
#define LOGFLUSHINTERVAL 3

namespace GsafetyAntDataAccess
{
	extern EventLoop* g_loop;
	muduo::AsyncLogging* g_asyncLog = NULL;

	void asyncOutput(const char* msg, int len)
	{
		g_asyncLog->append(msg, len);
	}

	void kill_signal_master(const int sig)
	{
		kill(0, SIGTERM);
		exit(0);
	}
	void kill_signal(const int sig)
	{
		exit(0);
	}

	void setmatsersignal()
	{
		::signal(SIGPIPE, SIG_IGN);
		::signal(SIGINT, kill_signal_master);
		::signal(SIGKILL, kill_signal_master);
		::signal(SIGQUIT, kill_signal_master);
		::signal(SIGTERM, kill_signal_master);
		::signal(SIGHUP, kill_signal_master);
		::signal(SIGSEGV, kill_signal_master);
	}
	void setchildsignal()
	{
		::signal(SIGINT, kill_signal);
		::signal(SIGKILL, kill_signal);
		::signal(SIGQUIT, kill_signal);
		::signal(SIGTERM, kill_signal);
		::signal(SIGHUP, kill_signal);
		::signal(SIGSEGV, kill_signal);
	}
}

int main(int argc, char* argv[])
{
#ifdef DAEMON
	daemon(1, 1);

	printf("\ndataAccess_monitor: pid = %d;tid = %d\n", ::getpid(), muduo::CurrentThread::tid());
	pid_t worker_pid_wait;
	pid_t worker_pid = fork();

	if (worker_pid < 0)
	{
		printf("Program Fork Failed..\n");
		exit(1);
	}
	if (worker_pid > 0)  // monitor (parent)
	{
		setmatsersignal();

		while (1)
		{
			worker_pid_wait = ::wait(NULL);
			if (worker_pid_wait < 0)
			{
				continue;
			}
			// child is terminated.
			usleep(1000000);
			worker_pid = fork();
			if (worker_pid < 0)
			{
				exit(1);
			}
			if (worker_pid == 0)
			{
				break;
			}
		}
	}
#endif

/*============================progress of work(child)==============================*/
	int idleSeconds;
	int mdvrport;
	int vedioport;
	int satcomport;
	int gpsfileport;
	int enableRaptorGps;
	string raptorGPSName;
	string version;

	//par setting
	printf("worker: pid = %d, tid = %d\n", ::getpid(), muduo::CurrentThread::tid());
	setchildsignal();
	// set max virtual memory to 2GB.
	rlimit rl = { 2*KONEGB, 2*KONEGB };
	setrlimit(RLIMIT_AS, &rl);

	//loading log class
	char name[256];
	strncpy(name, argv[0], LOGNAMELENGTH);
	muduo::AsyncLogging log(::basename(name), KROLLSIZE, LOGFLUSHINTERVAL);
	log.start(); usleep(100000);
	g_asyncLog = &log;
	muduo::Logger::setOutput(asyncOutput);

	Config cfg;
	try  // Read the file. If there is an error, report it and exit.
	{
		cfg.readFile("dataAccess.cfg");
	}
	catch(const FileIOException &fioex)
	{
		std::cerr << "I/O error while reading file." << std::endl;
		unlink(g_asyncLog->LogFileName_.c_str());
		return(EXIT_FAILURE);
	}
	catch(const ParseException &pex)
	{
		std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
			  << " - " << pex.getError() << std::endl;
		unlink(g_asyncLog->LogFileName_.c_str());
		return(EXIT_FAILURE);
	}

	try
	{
		string MQConfig = cfg.lookup("MQconfig");
		g_MQConfig = MQConfig;
		cout << "MQconfig: " << g_MQConfig << endl;

		string ver = cfg.lookup("Version");
		version = ver;
		cout << "Version: " << version << endl;

		string raptorname = cfg.lookup("RaptorGPSName");
		raptorGPSName = raptorname;
		cout << "raptorGPSName: " << raptorGPSName << endl;

		string MQGpsPrefix = cfg.lookup("MQGpsPrefix");
		g_MQGpsPrefix = MQGpsPrefix;
		cout << "MQGpsPrefix: " << g_MQGpsPrefix << endl;

		gpsfileport = cfg.lookup("GpsFileDownPort");
		cout << "GpsFileDownPort: " << gpsfileport << endl;

		idleSeconds = cfg.lookup("IdleSeconds");
		cout << "IdleSeconds: " << idleSeconds << endl;

		mdvrport = cfg.lookup("MdvrPort");
		cout << "MdvrPort: " << mdvrport << endl;

		vedioport = cfg.lookup("VedioPort");
		cout << "VedioPort: " << vedioport << endl;

		satcomport = cfg.lookup("SatcomPort");
		cout << "SatcomPort: " << satcomport << endl;

		enableRaptorGps = cfg.lookup("EnableRaptorGPS");
		cout << "EnableRaptorGPS: " << enableRaptorGps << endl;
	}
	catch(const SettingNotFoundException &nfex)
	{
		cerr << "No 'IdleSeconds' or 'MQconfig' setting in configuration file." << endl;
		unlink(g_asyncLog->LogFileName_.c_str());
		return(EXIT_FAILURE);
	}

	if (version.compare(VERSION))
	{
		cerr << "Version Error!!" << endl;
		kill(0, SIGTERM);
		return(EXIT_FAILURE);
	}

	/*
	 * Enable RaptorGPS Code
	 */
	if (enableRaptorGps)
	{
		char path[256] = {0};
		getcwd(path,sizeof(path));
		if (0 == fork())
		{
			if (path[strlen(path)-1] != '/')
				path[strlen(path)] = '/';
			strncpy(path+strlen(path),raptorGPSName.c_str(),raptorGPSName.length());
			if (-1 == execl(path, raptorGPSName.c_str(), NULL))
				LOG_ERROR << "raptorGPSProgram start Error:" << strerror(errno);
		}
	}

	EventLoop loop;
	g_loop = &loop;

	try
	{
		//from MDVR , Satcom GPS and VideoAccess
		InetAddress listenMDVRAddr(mdvrport);
		InetAddress listenVedioAddr(vedioport);
		InetAddress listenGpsAddr(satcomport);
		InetAddress	listenGpsFileAddr(gpsfileport);
		MdvrDataAccess server(&loop, listenMDVRAddr, listenVedioAddr, listenGpsAddr, listenGpsFileAddr, idleSeconds);
		(g_server = &server)->start();

		//MQ receive message
		muduo::Thread MQRcv(boost::bind(&MdvrDataAccess::threadRcvMQ, g_server), "MQReceiver");
		MQRcv.start();

#ifdef RAPTORGPS
		muduo::Thread RaptorRcv(boost::bind(&MdvrDataAccess::threadRaptorServer, &server),
		                   "RaptorGPSreceiver");
		RaptorRcv.start();
#endif
#ifdef SATCOMGPS
		muduo::Thread SatcomRcv(boost::bind(&MdvrDataAccess::threadSatcomServer, &server),
		                   "SatcomGPSreceiver");
		SatcomRcv.start();
#endif
		loop.loop();
	}catch(AMQPException& e)
	{
		cerr << e.getMessage() << endl;
		unlink(g_asyncLog->LogFileName_.c_str());
		return(EXIT_FAILURE);
	}catch(...)
	{
		cerr << "dataAccess Main Program run False;ErrorMsg unknow!" << endl;
	}
}

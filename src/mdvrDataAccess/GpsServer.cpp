/*
 * GpsServer.cpp
 *
 *  Created on: Dec 5, 2015
 *      Author: diwenjie
 *
 *  $GPRMC(Recommended Minimum Specific GPS/TRANSIT Data)
 *  格 式： $GPRMC,<1>,<2>,<3>,<4>,<5>,<6>,<7>,<8>,<9>,<10>,<11>,<12>*hh<CR><LF>
 *  	   $GPRMC,024813.640,A,3158.4608,N,11848.3737,E,10.05,324.27,150706,,,A*50
 *  说 明：
 *		字段 0：$GPRMC，语句ID，表明该语句为Recommended Minimum Specific GPS/TRANSIT Data（RMC）推荐最小定位信息
 *		字段 1：UTC时间，hhmmss.sss格式
 *		字段 2：状态，A=定位，V=未定位
 *		字段 3：纬度ddmm.mmmm，度分格式（前导位数不足则补0）
 *		字段 4：纬度N（北纬）或S（南纬）
 *		字段 5：经度dddmm.mmmm，度分格式（前导位数不足则补0）
 *		字段 6：经度E（东经）或W（西经）
 *		字段 7：速度，节，Knots（一节也是1.852千米／小时）
 *		字段 8：方位角，度（二维方向指向，相当于二维罗盘）
 *		字段 9：UTC日期，DDMMYY格式
 *		字段10：磁偏角，（000 - 180）度（前导位数不足则补0）
 *		字段11：磁偏角方向，E=东，W=西
 *		字段12：模式，A=自动，D=差分，E=估测，N=数据无效（3.0协议内容）
 *		字段13：校验值
 */

#include <string>
#include "GpsServer.h"
#include "mdvrServer.h"

using namespace muduo;
using namespace muduo::net;
using namespace std;

namespace GsafetyAntDataAccess
{
	std::string g_MQGpsPrefix; //"ANTGPS.V30."
	std::map<muduo::string, muduo::net::TcpConnectionPtr> g_GpsConnectMap;

	uint32_t get_uint32(const char* bytes, int len)
	{
	    if(len==4)
	        return (uint32_t)((bytes[0]<<24 & 0xff000000) | (bytes[1]<<16 & 0xff0000) | (bytes[2]<<8 & 0xff00) | (bytes[3] & 0xff));
	    if(len==3)
	        return (uint32_t)((bytes[0]<<16 & 0xff0000) | (bytes[1]<<8 & 0xff00) | (bytes[2] & 0xff));
	    return 0;
	}

	uint16_t get_uint16(const char* bytes, int len)
	{
	    if(len==2)
	        return (uint16_t)((bytes[0]<<8 & 0xff00) | (bytes[1] & 0xff));
	    return 0;
	}

	uint8_t get_uint8(const char* bytes, int len)
	{
	    if(len==1)
	        return (uint8_t)((bytes[0] & 0xff));
	    return 0;
	}

	void get_header_text(const char* buf, uint8_t* value, int value_size)
	{
	    int i=0, j=0;
	    for(i = 0; i < value_size; i++)
	    {
	        if(*(buf+i) != '\x20')
	        {
	            value[j] = (*(buf+i) & 0xFF);
	            j++;
	        }
	    }
	    value[j] = '\0';
	}

	//负数的补码转换
	void ComplementReverse(char* bytes, int nums)
	{
		for (int i = 0; i < nums; i++)
		{
			if (bytes[i] >> 31)
				bytes[i] = (~bytes[i]+1) & 0xFF;
		}
	}

	GpsServer::GpsServer()
	{
		// TODO Auto-generated constructor stub
	}

	GpsServer::~GpsServer()
	{
		// TODO Auto-generated destructor stub
	}

	void timeDecrease(SBDMessage *sbdmsg)
	{
		time_t secondsFrom1970 = 0;
		char* format="%d%m%y%H%M%S";
		struct tm time_info={0};
		char localtime[10] = {0};
		char localdate[10] = {0};
		string datetime = (char*)sbdmsg->MOPayloadInfo.Date;
		datetime += (char*)sbdmsg->MOPayloadInfo.Time;
		if(!strptime(datetime.c_str(), format, &time_info))
		{
			secondsFrom1970 = time(NULL);
			localtime_r(&secondsFrom1970, &time_info);
			strftime(localtime, sizeof localtime, "%H%M%S", &time_info);
			strftime(localdate, sizeof localtime, "%d%m%y", &time_info);
			memcpy(sbdmsg->MOPayloadInfo.Time, localtime, 6);
			memcpy(sbdmsg->MOPayloadInfo.Date, localdate, 6);
		}
		secondsFrom1970 = timelocal(&time_info);
		secondsFrom1970 -= 5 * 60 * 60;
		localtime_r(&secondsFrom1970, &time_info);
		strftime(localtime, sizeof localtime, "%H%M%S", &time_info);
		strftime(localdate, sizeof localtime, "%d%m%y", &time_info);
		memcpy(sbdmsg->MOPayloadInfo.Time, localtime, 6);
		memcpy(sbdmsg->MOPayloadInfo.Date, localdate, 6);
	}

	string timeIncrease(string datetime)
	{
		time_t secondsFrom1970 = 0;
		char* format="%d%m%y%H%M%S";
		struct tm time_info={0};
		char localtime[20] = {0};
		if(!strptime(datetime.c_str(), format, &time_info))
		{
			secondsFrom1970 = time(NULL);
			localtime_r(&secondsFrom1970, &time_info);
			strftime(localtime, sizeof localtime, "%H%M%S", &time_info);
			return localtime;
		}
		secondsFrom1970 = timelocal(&time_info);
		secondsFrom1970 += 5 * 60 * 60;
		localtime_r(&secondsFrom1970, &time_info);
		strftime(localtime, sizeof localtime, "%H%M%S", &time_info);
		return localtime;
	}

	void MdvrDataAccess::onGpsConnection(const muduo::net::TcpConnectionPtr& conn)
	{
		LOG_INFO << "GpsAccessServer --- " << conn->peerAddress().toIpPort() << " -> "
			<< conn->localAddress().toIpPort() << " is " << (conn->connected() ? "UP" : "DOWN");
	}

	/*
	 * Functional logic based on <IRDM_IridiumSBDService_V3_DEVGUIDE_9Mar2012> version development;
	 * Access to the two types of equipment data include(GPRMC,iridium);
	 */
	void MdvrDataAccess::onGpsMessage(const muduo::net::TcpConnectionPtr& conn,
			muduo::net::Buffer* buf, muduo::Timestamp time)
	{
		LOG_INFO << "GpsAccessServer --- " << conn->peerAddress().toIpPort() <<
			";Recv Timestamp->" << time.toFormattedString();

		size_t length = buf->readableBytes();
		string revmsg = buf->retrieveAllAsString();

		gpsDataAnalysis(revmsg, length);
	}

	//A byte unzip into two bytes;char to hex
	ErrCode get_payload_text(SBDMessage *sbdmsg, const char* buf, uint16_t length)
	{
		int i = 0, j = 0;
	    uint8_t highByte, lowByte;
		uint8_t decodeData[2048] = {0};

		for (; i < length; )
		{
			if (0 == j && (uint8_t)buf[i++] != 0xAA)
				continue;

			if ((uint8_t)buf[i] == 0xBB)
				break;

			highByte = (buf[i] & 0xFF) >> 4;
			lowByte = buf[i++] & 0x0F;

			highByte += 0x30;

			if (highByte > 0x39)
				decodeData[j * 2] = highByte + 0x07;
			else
				decodeData[j * 2] = highByte;

			if (decodeData[j * 2] == 0x46)
				decodeData[j * 2] = 0x30;

			lowByte += 0x30;
			if (lowByte > 0x39)
				decodeData[j * 2 + 1] = lowByte + 0x07;
			else
				decodeData[j * 2 + 1] = lowByte;

			if (decodeData[j * 2 + 1] == 0x46)
				decodeData[j * 2 + 1] = 0x30;

			j++;
		}

		if (0x0 == j && 0x0 != i) return MO_PAYLOAD_PACKET_HEADER_ERROR;
		if (length == i) return MO_PAYLOAD_PACKET_TAIL_ERROR;
		if (0x17 != j && 0x09 != j) return MO_PAYLOAD_PACKET_INVALID;

		uint16_t pos = 0;
		memcpy(sbdmsg->MOPayloadInfo.Equidentifcation, decodeData + pos, 5);
		pos += 5;
		memcpy(sbdmsg->MOPayloadInfo.Time, decodeData + pos, 6);
		pos += 6;
		if (0x17 == j)
		{
			memcpy(sbdmsg->MOPayloadInfo.Latitude, decodeData + pos, 4);
			pos += 4;
			sbdmsg->MOPayloadInfo.Latitude[4] = '.';
			memcpy(sbdmsg->MOPayloadInfo.Latitude + 5, decodeData + pos, 3);
			pos += 3;
			if (*(decodeData + pos) == 0x41)
				sbdmsg->MOPayloadInfo.LatitudeLocation = 0x53; //S
			else
				sbdmsg->MOPayloadInfo.LatitudeLocation = 0x4E; //N
			pos++;
			memcpy(sbdmsg->MOPayloadInfo.Longitude, decodeData + pos, 5);
			pos += 5;
			sbdmsg->MOPayloadInfo.Longitude[5] = '.';
			memcpy(sbdmsg->MOPayloadInfo.Longitude + 6, decodeData + pos, 3);
			pos += 3;
			if (*(decodeData + pos) == 0x43)
				sbdmsg->MOPayloadInfo.LongitudeLocation = 0x57; //W
			else
				sbdmsg->MOPayloadInfo.LongitudeLocation = 0x45; //E
			pos++;
			memcpy(sbdmsg->MOPayloadInfo.CarSpeed, decodeData + pos, 3);
			pos += 3;
			memcpy(sbdmsg->MOPayloadInfo.Azimuth, decodeData + pos, 3);
			pos += 3;
		}
		memcpy(sbdmsg->MOPayloadInfo.Date, decodeData + pos, 6);
		pos += 6;
		sbdmsg->MOPayloadInfo.Alarm = *(decodeData + pos);
		pos++;
		if (0x17 == j)
		{
			memcpy(sbdmsg->MOPayloadInfo.Height, decodeData + pos, 5);
		}

		timeDecrease(sbdmsg); //GMT->local

		return MO_PAYLOAD_PACKET_NO_ERROR;
	}

	/*
	 * Element Identifiers
	 */
	void MdvrDataAccess::gpsDataAnalysis(string const& gpsData, size_t length)
	{
		string IMEI;
		string msg;
		if (gpsData[0] == 0x24) //$GPRMC,*
		{
			/*
			 * other GPGGA/GPGSV...
			 * 3.0=模式(A/D/E/N)，A=自动，D=差分，E=估测，N=数据无效
			 * 纬度 保留8字节，经度 保留9字节
			 * other process function
			 * int find(char c, int pos = 0) const; //从pos开始查找字符c在当前字符串的位置
			 * string &replace(int p0, int n0,const char *s);//删除从p0开始的n0个字符，然后在p0处插入串s
			 */
			msg = gpsData;
			LOG_INFO << "From Satcom DevInfo:" << msg;
			if (gpsData.compare(0, 7, "$GPRMC,"))
			{
				LOG_WARN << "GPRMC Data Header Invalid!!";
				return;
			}
			try {
				IMEI = split(gpsData, ',', 2);
				string gpsstat = split(gpsData, ',', 4);
				if (gpsstat.compare("A"))
				{
					LOG_WARN << "GPRMC Data Itself Invalid!!";
					return;
				}
				msg.erase(6, IMEI.length() + 1);
				string::iterator first = find_n_c_noconst(msg, ',', 3)+9;
				string::iterator second = find_n_c_noconst(msg, ',', 4);
				if (first < second)
					msg.erase(first, second);

				first = find_n_c_noconst(msg, ',', 5)+10;
				second = find_n_c_noconst(msg, ',', 6);
				if (first < second)
					msg.erase(first, second);

				string::iterator itor = std::find(msg.begin(), msg.end(), '*');
				//防止已经有模式位再次被添加
				if (itor[-1] == 'W' || itor[-1] == 'E')
					msg.replace(itor, itor, ",A"); //3.0协议内容
				msg += ',' + IMEI + ",00000,0#"; //后两位是海拔和报警信息
			}catch(std::domain_error msg)
			{
				LOG_WARN << msg.what();
				return;
			}
		}
		else if (gpsData[0] == 0x01) //MO Header IEI
		{
			SBDMessage sbdmsg = {0};
			const char *buffer = gpsData.c_str();
			size_t position = 0;
			int status = -1;

			sbdmsg.ProtocolRevisionNumber = get_uint8(buffer + position, 1);
			position++;
			sbdmsg.OverallMessageLength = get_uint16(buffer + position, 2);
			position += 2;

			while(position < length)
			{
				switch(get_uint8(buffer + position,1))
				{
					case 0x01: ///MO Header Information IEI
						sbdmsg.MOHeaderIEI = get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOHeaderLength = get_uint16(buffer + position, 2);
						position += 2;
						//ComplementReverse((char*)buffer + position,6);
						sbdmsg.CDRReference = get_uint32(buffer + position, 4);
						position += 4;
						get_header_text(buffer + position, sbdmsg.IMEI, 15);
						position += 15;
						sbdmsg.SessionStatus = get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOMSN = get_uint16(buffer + position, 2);
						position += 2;
						sbdmsg.MTMSN = get_uint16(buffer + position, 2);
						position += 2;
						sbdmsg.TimeofSession = get_uint32(buffer + position, 4);
						position += 4;
						break;
					case 0x03: //MO Location Information IEI
						sbdmsg.MOLocationInfo.MOLocationInfoIEI = get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOLocationInfo.MOLocationInfoLength = get_uint16(buffer + position, 2);
						position += 2;
						sbdmsg.MOLocationInfo.NSIAndEWI = get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOLocationInfo.LatitudeDegrees = get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOLocationInfo.Latitude = get_uint16(buffer + position, 2);
						position += 2;
						sbdmsg.MOLocationInfo.LongitudeDegrees = get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOLocationInfo.Longitude = get_uint16(buffer + position, 2);
						position += 2;
						sbdmsg.MOLocationInfo.CEPRadius = get_uint32(buffer + position, 4);
						position += 4;
						break;
					case 0x02: //MO Payload Information IEI
						sbdmsg.MOPayloadInfo.MOPayloadIEI= get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOPayloadInfo.MOPayloadLength = get_uint16(buffer + position, 2); //25 or 11
						position += 2;
						status = get_payload_text(&sbdmsg, buffer + position, sbdmsg.MOPayloadInfo.MOPayloadLength);
						if (status != MO_PAYLOAD_PACKET_NO_ERROR)
						{
							LOG_INFO << "Device IMEI:" << sbdmsg.IMEI << ";MO Payload Data Parase Error:" << status;
							return;
						}
						position += sbdmsg.MOPayloadInfo.MOPayloadLength;
						break;
					case 0x05: //MO Confirmation Information IEI
						//no Analysis
						sbdmsg.MOConfirmationInfo.MOConfirmationInfoIEI = get_uint8(buffer + position, 1);
						position++;
						sbdmsg.MOConfirmationInfo.MOConfirmationInfoLength = get_uint16(buffer + position, 2);
						position += 2;
						sbdmsg.MOConfirmationInfo.MOConfirmationStatus = get_uint8(buffer + position, 1);
						position++;
						break;
					default:
						LOG_WARN << "@@@(GPS Data Itself Invalid)@@@";
						return;
				}
			}

			IMEI = (char*)sbdmsg.IMEI;
			msg = "$GPRMC,";
			msg += (char*)sbdmsg.MOPayloadInfo.Time;
			if (sbdmsg.MOPayloadInfo.MOPayloadLength == 0x19)
				msg += ",A,";
			else if (sbdmsg.MOPayloadInfo.MOPayloadLength == 0x0B)
				msg += ",V,";
			else
			{
				LOG_WARN << "!!(GPS Data PayloadLength Invalid)!!";
				return;
			}
			msg += (char*)sbdmsg.MOPayloadInfo.Latitude;
			msg += ",";
			if (sbdmsg.MOPayloadInfo.MOPayloadLength == 0x19)
				msg += sbdmsg.MOPayloadInfo.LatitudeLocation;
			msg += ',';
			msg += (char*)sbdmsg.MOPayloadInfo.Longitude;
			msg += ",";
			if (sbdmsg.MOPayloadInfo.MOPayloadLength == 0x19)
				msg += sbdmsg.MOPayloadInfo.LongitudeLocation;
			msg += ',';
			msg += (char*)sbdmsg.MOPayloadInfo.CarSpeed;
			msg += ",";
			msg += (char*)sbdmsg.MOPayloadInfo.Azimuth;
			msg += ",";
			msg += (char*)sbdmsg.MOPayloadInfo.Date;
			if (sbdmsg.MOPayloadInfo.MOPayloadLength == 0x19)
			{
				msg += ",,,A*3D," + IMEI + ",";
				msg += (char*)sbdmsg.MOPayloadInfo.Height;
			}
			else
			{
				msg += ",,,N*3D," + IMEI + ",00000";
			}
			msg += ",";
			msg += sbdmsg.MOPayloadInfo.Alarm;
			msg += "#";
		}
		else
		{
			LOG_WARN << "@@@(GPS Data Header Invalid)@@@";
			return;
		}

		string key = g_MQGpsPrefix + IMEI;
		send3MQ(exGps_, msg, key);
		//send2MQ(exGps_, msg, key);
	}
}


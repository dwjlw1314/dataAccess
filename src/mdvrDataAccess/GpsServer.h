/*
 * GpsServer.h
 *
 *  Created on: Dec 5, 2015
 *      Author: diwenjie
 *
 * MOPayloadInfomation.Alarm 含义
 * 代码		描述
 *  1	GPS信号丢失
 *  2	声音报警启动（输出为高）
 *  3	声音报警关闭（输出为低）
 *  4	车辆屏蔽启动（输出为高）
 *  5	车辆屏蔽关闭（输出为低）
 *  6	车辆保险装置启动—打开门（输出为高）
 *  7	车辆保险装置启动—关闭门（输出为高）
 *  8	未使用
 *  9	一键报警键已按下（输入为高）
 *  A	没点火发生移动的报警（引擎ACC OFF）
 *  B	未连接的设备/无外部供电（输入为低）
 *  C	已连接的设备/有外部供电（输入为高）
 *  D	点火熄灭（ACC OFF-输出为低）
 *  E	点火启动（ACC ON-输出为高）
 *  F	未使用
 */

#ifndef MDVRDATAACCESS_GPSSERVER_H_
#define MDVRDATAACCESS_GPSSERVER_H_

#include <stdint.h>

namespace GsafetyAntDataAccess
{

	typedef enum errStatus
	{
		MO_PAYLOAD_PACKET_NO_ERROR,
		MO_PAYLOAD_PACKET_HEADER_ERROR = 1001,
		MO_PAYLOAD_PACKET_TAIL_ERROR,
		MO_PAYLOAD_PACKET_INVALID
	}ErrCode;

	//MO Location Information
	typedef struct MOLocationInfomation
	{
		uint8_t MOLocationInfoIEI;
		/*
		 * NSI: North/South Indicator (0=North, 1=South) 1 byte
		 * EWI: East/West Indicator (0=East, 1=West) 0 byte
		 */
		uint8_t NSIAndEWI;
		uint8_t LatitudeDegrees;
		uint8_t LongitudeDegrees;

		uint16_t MOLocationInfoLength;
		uint16_t Latitude; //(American notation)
		uint16_t Longitude; //(American notation)

		uint32_t CEPRadius;
	}MOLocationInfomation;

	//MO Payload;have 25 Bytes and 11 Bytes kinds After decompression double;
	typedef struct MOPayloadInfomation
	{
		uint8_t MOPayloadIEI;
		uint8_t Equidentifcation[6];
		uint8_t Time[7];
		uint8_t Latitude[9]; //have .
		uint8_t LatitudeLocation;
		uint8_t Longitude[10]; //have .
		uint8_t LongitudeLocation;
		uint8_t CarSpeed[4];
		uint8_t Azimuth[4]; //方位
		uint8_t Date[7];
		uint8_t Alarm;
		uint8_t Height[6];

		uint16_t MOPayloadLength;
		//uint8_t MOPayloadData[47];
	}MOPayloadInfomation;

	//MO Confirmation Message
	typedef struct MOConfirmationInfomation
	{
		uint8_t MOConfirmationInfoIEI;
		uint8_t MOConfirmationStatus;
		uint16_t MOConfirmationInfoLength;
	}MOConfirmationInfomation;

	/*
	 * iridium type data structure
	 */
	typedef struct SBDMessage
	{
		uint8_t ProtocolRevisionNumber;
		uint8_t MOHeaderIEI;
		uint8_t IMEI[16]; //15 Byte add 1 Byte
		uint8_t SessionStatus;

		uint16_t OverallMessageLength;
		uint16_t MOHeaderLength;
		uint16_t MOMSN;
		uint16_t MTMSN;

		uint32_t TimeofSession;
		uint32_t CDRReference;

		MOLocationInfomation MOLocationInfo;
		MOPayloadInfomation MOPayloadInfo;
		MOConfirmationInfomation MOConfirmationInfo;
	}SBDMessage;

	class GpsServer
	{
	public:
		GpsServer();
		virtual ~GpsServer();
	};
}

#endif /* MDVRDATAACCESS_GPSSERVER_H_ */

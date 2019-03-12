/*
 * cmdDefine.h
 *
 *  Created on: Aug 13, 2013
 *      Author: root
 */

#ifndef CMDDEFINE_H_
#define CMDDEFINE_H_

/* Function of the switch */
#define VERSION "2.0.0.2"
//#define RAPTORGPS
//#define SATCOMGPS
#define DAEMON
//#define FIREALERT
//#define POWEROFF


/* Protocol */
#define CMD_RESP_USER							"V0"
#define CMD_REGISTER							"V1"

#define CMD_GPS_FILE_DOWN						"V4"

#ifdef POWEROFF
#define CMD_POWEROFF							"V20"
#endif

#define CMD_VEDIO_LINK_CREATE_FAIL				"V21"
#define CMD_VEDIO_SESSION_START					"V22"
#define CMD_VEDIO_SESSION_END					"V23"

#define CMD_GPS									"V30"
#define CMD_MDVR_STATE							"V39"

#define CMD_ONEKEY_ALARM						"V61"
#define CMD_CAMERA_NOSIGNAL_ALARM				"V63"
#define CMD_CAMERA_SHIELD_ALARM					"V64"
#define CMD_TEMP_ALARM							"V68"
#define CMD_SDCARD_ERROR_ALARM					"V69"


#define CMD_OVER_SPEED_ALERT					"V70"
#define CMD_GPS_RECEIVER_ALERT					"V75"
#define CMD_ALERT_REMOVE						"V77"
#define CMD_VOLTAGE_ALERT						"V78"
#define CMD_AREA_ALERT							"V79"

#ifdef FIREALERT
#define CMD_FIRE_ALERT							"V82"
#endif

#define CMD_DOOR_ALERT							"V83"

#define CMD_XML									"V600"

#define CMD_RESP_CONTENT_SORF					",0,1,#" // maybe need modify
#define CMD_RESP_CONTENT_EMPTY					",0,#"

#endif /* CMDDEFINE_H_ */

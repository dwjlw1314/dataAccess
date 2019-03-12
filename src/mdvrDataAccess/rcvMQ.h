/*
 * rcvMQ.h
 *
 *  Created on: Aug 17, 2013
 *      Author: root
 */

#ifndef RCVMQ_H_
#define RCVMQ_H_

#include "includes.h"
#include "AMQP/AMQPcpp.h"

namespace GsafetyAntDataAccess
{
	void threadRcvMQ();
	int onCancel(AMQPMessage * message);
	int onMessage(AMQPMessage * message);
}
#endif /* RCVMQ_H_ */

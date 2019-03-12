include_directories(/usr/local/rabbitmq-c-master/include/)
include_directories(AMQP/)
link_directories(/usr/local/rabbitmq-c-master/lib/)
add_executable(mdvrDataAccess mdvrServer.cc main.cc 
	 util.cc cmdHandle.cc 
	 AMQP/AMQP.cc AMQP/AMQPBase.cc AMQP/AMQPException.cc AMQP/AMQPExchange.cc AMQP/AMQPMessage.cc AMQP/AMQPQueue.cc
	 rcvMQ.cc vedioServer.cc log.cc)
target_link_libraries(mdvrDataAccess muduo_net rabbitmq)
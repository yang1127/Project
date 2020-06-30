#include "Client.h"
#include "Server.h"

void ClientRun()
{
	Client cli;
	cli.Start();  
}

int main(int argc, char*argv[])
{
	std::thread thr_client(ClientRun); //创建一个线程运行客户端模块，主线程运行服务端模块

	Server srv;
	srv.Start();

	system("pause");
	return 0;
}
#include "Client.h"
#include "Server.h"

void ClientRun()
{
	Client cli;
	cli.Start();  
}

int main(int argc, char*argv[])
{
	std::thread thr_client(ClientRun); //����һ���߳����пͻ���ģ�飬���߳����з����ģ��

	Server srv;
	srv.Start();

	system("pause");
	return 0;
}
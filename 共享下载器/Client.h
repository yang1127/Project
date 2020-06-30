#pragma once
#include <thread> 
#include <boost/filesystem.hpp>
#include "Util.h"
#include "httplib.h"
#define PKQ_PORT 9000 //Ƥ����˿���Ϣ
#define MAX_IPBUFFER 16
#define MAX_RANGE  (100*1024*1024)
#define SHARED_PATH "./Shared/" //����Ŀ¼
#define DOWNLOAD_PATH "./Download/" //����·��

class Host
{
public:
	uint32_t  _ip_addr; //Ҫ��Ե�����IP��ַ
	bool  _pair_ret;    //���ڴ����Խ������Գɹ���Ϊtrue��ʧ����Ϊfalse��
};

class Client
{
public:
	bool Start()
	{
		while (1)
		{
			GetOnlineHost();
		}
		return true;
	}

	void HostPair(Host *host) //������Ե��߳���ں���
	{
		//1.��֯httpЭ���ʽ����������
		//2.�һ��tcp�Ŀͻ��ˣ������ݷ���
		//3.�ȴ�����˵Ļظ��������н���

		host->_pair_ret = false;
		char buf[MAX_IPBUFFER] = { 0 }; 
		inet_ntop(AF_INET, &host->_ip_addr, buf, MAX_IPBUFFER);  
		httplib::Client cli(buf, PKQ_PORT); //ʵ����һ��httplib�Ŀͻ��˶���
		auto rsp = cli.Get("/hostpair");    //�����˷�����ԴΪ/hostpair��Get����

		if (rsp && rsp->status == 200) 
		{
			host->_pair_ret = true;  
		}

		return;
	}

	//1.��ȡ��������
	bool GetOnlineHost() 
	{
		char ch = 'Y'; 
		if (!_online_host.empty())
		{
			std::cout << "�Ƿ�����ƥ����������(Y/N):";    
			fflush(stdout); 
			std::cin >> ch; 
		}

		if (ch == 'Y')
		{
			std::cout << "��ʼ����ƥ��...\n";

			std::vector<Adapter> list;
			AdapterUtil::GetAllAdapter(&list); //��ȡ������Ϣ
			std::vector<Host> host_list; //��ȡ��������IP��ַ

			//�õ����е�����IP��ַ�б�
			for (int i = 0; i < list.size(); ++i) 
			{
				uint32_t ip = list[i]._ip_addr;
				uint32_t mask = list[i]._mask_addr;
				uint32_t net = (ntohl(ip & mask)); //�����:ip��ַ&��������
				int32_t max_host = (~ntohl(mask)); //���������:��������ȡ��

				for (int j = 1; j < max_host; ++j)
				{
					uint32_t host_ip = net + j;
					Host host; 
					host._ip_addr = htonl(host_ip); //�������ֽ����IP��ַת��Ϊ�����ֽ���
					host._pair_ret = false; 
					host_list.push_back(host); 
				}
			}

			//�����IP��ַ�б��е����������������
			std::vector<std::thread*> thr_list(host_list.size()); //��host_list�е����������߳̽������
			for (int i = 0; i < host_list.size(); ++i)
			{
				thr_list[i] = new std::thread(&Client::HostPair, this, &host_list[i]);
			}

			std::cout << "��������ƥ���У����Ժ�....\n";
			//������������ӵ�online_host��
			for (int i = 0; i < host_list.size(); i++) 
			{
				thr_list[i]->join(); //�ȴ��߳��˳�
				if (host_list[i]._pair_ret == true)
				{
					_online_host.push_back(host_list[i]);
				}
				delete thr_list[i]; 
			}
		}

		//�����е�����������IP��ӡ������������ѡ��
		for (int i = 0; i < _online_host.size(); ++i)
		{
			char buf[MAX_IPBUFFER] = { 0 };
			inet_ntop(AF_INET, &_online_host[i]._ip_addr, buf, MAX_IPBUFFER); 
			std::cout << "\t" << buf << std::endl; 
		}

		std::cout << "���û�ѡ�������������ȡ�����ļ��б�:";
		fflush(stdout); 
		std::string select_ip; //�û�ѡ������
		std::cin >> select_ip;
		GetShareList(select_ip); //�û�ѡ��֮�󣬵��û�ȡ�ļ��б�ӿ�

		return true;
	}

	//2.��ȡ�ļ��б�
	bool GetShareList(const std::string &host_ip)
	{
		//�����˷���һ���ļ��б��ȡ����
		httplib::Client cli(host_ip.c_str(), PKQ_PORT);
		auto rsp = cli.Get("/list"); 
		if (rsp == NULL || rsp->status != 200) 
		{
			std::cerr << "��ȡ�ļ��б���Ӧ����\n";
			return false;
		}

		//��ӡ�������Ӧ���ļ������б��û�ѡ��
		std::cout << rsp->body << std::endl;
		std::cout << "\n��ѡ��Ҫ���ص��ļ�:";
		fflush(stdout);
		std::string filename;
		std::cin >> filename;
		RangeDownload(host_ip, filename);

		return true;
	}

	//3.1 ֱ�������ļ�
	bool DownloadFile(const std::string &host_ip, const std::string& filename)
	{
		//1.�����˷����ļ���������
		//2.�õ���Ӧ�������Ӧ�е�body���ľ����ļ�����
		//3.�����ļ������ļ�д���ļ��У��ر��ļ�

		std::string req_path = "/download/" + filename;  
		httplib::Client cli(host_ip.c_str(), PKQ_PORT);

		auto rsp = cli.Get(req_path.c_str());
		if (rsp == NULL || rsp->status != 200)
		{
			std::cerr << "�����ļ���ȡ��Ӧʧ��/n";
			return false;
		}

		if (!boost::filesystem::exists(DOWNLOAD_PATH)) 
		{
			boost::filesystem::create_directory(DOWNLOAD_PATH);
		}

		std::string realpath = DOWNLOAD_PATH + filename;
		if (FileUtil::Write(realpath, rsp->body) == false)
		{
			std::cerr << "�ļ�����ʧ��\n";
			return false;
		}
		std::cout << "�����ļ��ɹ�\n";
		return true;
	}

	//3.2 �ֿ������ļ�
	bool RangeDownload(const std::string &host_ip, const std::string &name)
	{
		//����HEAD����ͨ����Ӧ�е�Content-Length��ȡ�ļ���С
		std::string req_path = "/download/" + name;
		httplib::Client cli(host_ip.c_str(), PKQ_PORT);

		auto rsp = cli.Head(req_path.c_str());
		if (rsp == NULL || rsp->status != 200)
		{
			std::cout << "��ȡ�ļ���С��Ϣʧ��\n";
			return false;
		}

		std::string clen = rsp->get_header_value("Content-Length"); 
		int32_t filesize = StringUtil::Str2Dig(clen); 

		//2.�����ļ���С���зֿ�
		//a. ���ļ���С С�ڿ��С����ֱ�������ļ�
		if (filesize < MAX_RANGE)
		{
			std::cout << "�ļ���С,ֱ�������ļ�\n";
			return DownloadFile(host_ip, name);
		}
		//����ֿ����
		//b. ���ļ���С�����������С����ֿ����λ�ļ���С���Էֿ��СȻ��+1
		//c. ���ļ���С�պ��������С����ֿ���������ļ���С���Էֿ��С
		std::cout << "�ļ��Ƚϴ�,�ֿ�����\n";
		int range_count = 0;
		if ((filesize % MAX_RANGE) == 0) //b
		{
			range_count = filesize / MAX_RANGE;
		}
		else //c
		{
			range_count = (filesize / MAX_RANGE) + 1;
		}

		int32_t range_start = 0, range_end = 0; 
		for (int i = 0; i < range_count; i++)
		{
			range_start = i * MAX_RANGE;
			if (i == (range_count - 1)) //i=1 ���һ���ֿ�
			{
				range_end = filesize - 1;
			}
			else
			{
				range_end = ((i + 1) * MAX_RANGE) - 1;
			}

			//3.��һ����ֿ����ݣ��õ���Ӧ֮��д���ļ�ָ��λ��
			std::stringstream tmp;
			tmp << "bytes=" << range_start << "-" << range_end; //��֯һ��Rangeͷ��Ϣ����ֵ
			httplib::Headers header;
			header.insert(std::make_pair("Range", tmp.str())); //����һ��range����

			auto rsp = cli.Get(req_path.c_str(), header);
			if (rsp == NULL || rsp->status != 206)//����ʧ��
			{
				std::cout << "���������ļ�ʧ��\n";
				return false;
			}
			std::string real_path = DOWNLOAD_PATH + name;
			if (!boost::filesystem::exists(DOWNLOAD_PATH))
			{
				boost::filesystem::create_directory(DOWNLOAD_PATH);
			}

			//��ȡ�ļ��ɹ������ļ��зֿ�д������
			FileUtil::Write(real_path, rsp->body, range_start);
		}
		std::cout << "�ļ����سɹ�!" << std::endl;
	}

private:
	std::vector<Host> _online_host;
};
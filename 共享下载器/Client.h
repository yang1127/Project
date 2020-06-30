#pragma once
#include <thread> 
#include <boost/filesystem.hpp>
#include "Util.h"
#include "httplib.h"
#define PKQ_PORT 9000 //皮卡丘端口信息
#define MAX_IPBUFFER 16
#define MAX_RANGE  (100*1024*1024)
#define SHARED_PATH "./Shared/" //共享目录
#define DOWNLOAD_PATH "./Download/" //下载路径

class Host
{
public:
	uint32_t  _ip_addr; //要配对的主机IP地址
	bool  _pair_ret;    //用于存放配对结果，配对成功则为true，失败则为false；
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

	void HostPair(Host *host) //主机配对的线程入口函数
	{
		//1.组织http协议格式的请求数据
		//2.搭建一个tcp的客户端，将数据发送
		//3.等待服务端的回复，并进行解析

		host->_pair_ret = false;
		char buf[MAX_IPBUFFER] = { 0 }; 
		inet_ntop(AF_INET, &host->_ip_addr, buf, MAX_IPBUFFER);  
		httplib::Client cli(buf, PKQ_PORT); //实例化一个httplib的客户端对象
		auto rsp = cli.Get("/hostpair");    //向服务端发送资源为/hostpair的Get请求

		if (rsp && rsp->status == 200) 
		{
			host->_pair_ret = true;  
		}

		return;
	}

	//1.获取在线主机
	bool GetOnlineHost() 
	{
		char ch = 'Y'; 
		if (!_online_host.empty())
		{
			std::cout << "是否重新匹配在线主机(Y/N):";    
			fflush(stdout); 
			std::cin >> ch; 
		}

		if (ch == 'Y')
		{
			std::cout << "开始主机匹配...\n";

			std::vector<Adapter> list;
			AdapterUtil::GetAllAdapter(&list); //获取网卡信息
			std::vector<Host> host_list; //获取所有主机IP地址

			//得到所有的主机IP地址列表
			for (int i = 0; i < list.size(); ++i) 
			{
				uint32_t ip = list[i]._ip_addr;
				uint32_t mask = list[i]._mask_addr;
				uint32_t net = (ntohl(ip & mask)); //网络号:ip地址&子网掩码
				int32_t max_host = (~ntohl(mask)); //最大主机号:子网掩码取反

				for (int j = 1; j < max_host; ++j)
				{
					uint32_t host_ip = net + j;
					Host host; 
					host._ip_addr = htonl(host_ip); //将主机字节序的IP地址转换为网络字节序
					host._pair_ret = false; 
					host_list.push_back(host); 
				}
			}

			//逐个对IP地址列表中的主机发送配对请求
			std::vector<std::thread*> thr_list(host_list.size()); //对host_list中的主机创建线程进行配对
			for (int i = 0; i < host_list.size(); ++i)
			{
				thr_list[i] = new std::thread(&Client::HostPair, this, &host_list[i]);
			}

			std::cout << "正在主机匹配中，请稍后....\n";
			//将在线主机添加到online_host中
			for (int i = 0; i < host_list.size(); i++) 
			{
				thr_list[i]->join(); //等待线程退出
				if (host_list[i]._pair_ret == true)
				{
					_online_host.push_back(host_list[i]);
				}
				delete thr_list[i]; 
			}
		}

		//将所有的在线主机的IP打印出来，供主机选择
		for (int i = 0; i < _online_host.size(); ++i)
		{
			char buf[MAX_IPBUFFER] = { 0 };
			inet_ntop(AF_INET, &_online_host[i]._ip_addr, buf, MAX_IPBUFFER); 
			std::cout << "\t" << buf << std::endl; 
		}

		std::cout << "请用户选择配对主机，获取共享文件列表:";
		fflush(stdout); 
		std::string select_ip; //用户选择主机
		std::cin >> select_ip;
		GetShareList(select_ip); //用户选择之后，调用获取文件列表接口

		return true;
	}

	//2.获取文件列表
	bool GetShareList(const std::string &host_ip)
	{
		//向服务端发送一个文件列表获取请求
		httplib::Client cli(host_ip.c_str(), PKQ_PORT);
		auto rsp = cli.Get("/list"); 
		if (rsp == NULL || rsp->status != 200) 
		{
			std::cerr << "获取文件列表响应错误\n";
			return false;
		}

		//打印服务端响应的文件名称列表供用户选择
		std::cout << rsp->body << std::endl;
		std::cout << "\n请选择要下载的文件:";
		fflush(stdout);
		std::string filename;
		std::cin >> filename;
		RangeDownload(host_ip, filename);

		return true;
	}

	//3.1 直接下载文件
	bool DownloadFile(const std::string &host_ip, const std::string& filename)
	{
		//1.向服务端发送文件下载请求
		//2.得到响应结果，响应中的body正文就是文件数据
		//3.创建文件，将文件写入文件中，关闭文件

		std::string req_path = "/download/" + filename;  
		httplib::Client cli(host_ip.c_str(), PKQ_PORT);

		auto rsp = cli.Get(req_path.c_str());
		if (rsp == NULL || rsp->status != 200)
		{
			std::cerr << "下载文件获取响应失败/n";
			return false;
		}

		if (!boost::filesystem::exists(DOWNLOAD_PATH)) 
		{
			boost::filesystem::create_directory(DOWNLOAD_PATH);
		}

		std::string realpath = DOWNLOAD_PATH + filename;
		if (FileUtil::Write(realpath, rsp->body) == false)
		{
			std::cerr << "文件下载失败\n";
			return false;
		}
		std::cout << "下载文件成功\n";
		return true;
	}

	//3.2 分块下载文件
	bool RangeDownload(const std::string &host_ip, const std::string &name)
	{
		//发送HEAD请求，通过响应中的Content-Length获取文件大小
		std::string req_path = "/download/" + name;
		httplib::Client cli(host_ip.c_str(), PKQ_PORT);

		auto rsp = cli.Head(req_path.c_str());
		if (rsp == NULL || rsp->status != 200)
		{
			std::cout << "获取文件大小信息失败\n";
			return false;
		}

		std::string clen = rsp->get_header_value("Content-Length"); 
		int32_t filesize = StringUtil::Str2Dig(clen); 

		//2.根据文件大小进行分块
		//a. 若文件大小 小于块大小，则直接下载文件
		if (filesize < MAX_RANGE)
		{
			std::cout << "文件较小,直接下载文件\n";
			return DownloadFile(host_ip, name);
		}
		//计算分块个数
		//b. 若文件大小不能整除块大小，则分块个数位文件大小除以分块大小然后+1
		//c. 若文件大小刚好整除块大小，则分块个数就是文件大小除以分块大小
		std::cout << "文件比较大,分块下载\n";
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
			if (i == (range_count - 1)) //i=1 最后一个分块
			{
				range_end = filesize - 1;
			}
			else
			{
				range_end = ((i + 1) * MAX_RANGE) - 1;
			}

			//3.逐一请求分块数据，得到响应之后写入文件指定位置
			std::stringstream tmp;
			tmp << "bytes=" << range_start << "-" << range_end; //组织一个Range头信息区间值
			httplib::Headers header;
			header.insert(std::make_pair("Range", tmp.str())); //设置一个range区间

			auto rsp = cli.Get(req_path.c_str(), header);
			if (rsp == NULL || rsp->status != 206)//请求失败
			{
				std::cout << "区间下载文件失败\n";
				return false;
			}
			std::string real_path = DOWNLOAD_PATH + name;
			if (!boost::filesystem::exists(DOWNLOAD_PATH))
			{
				boost::filesystem::create_directory(DOWNLOAD_PATH);
			}

			//获取文件成功，向文件中分块写入数据
			FileUtil::Write(real_path, rsp->body, range_start);
		}
		std::cout << "文件下载成功!" << std::endl;
	}

private:
	std::vector<Host> _online_host;
};
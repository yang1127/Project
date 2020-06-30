#pragma once
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <fstream> 
#include <boost/filesystem.hpp>
#include <sstream>
#ifdef _WIN32 
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Iphlpapi.h> //获取网卡信息接口的头文件
#pragma comment(lib, "Iphlpapi.lib") //获取网卡信息接口的库文件包含
#pragma comment(lib, "ws2_32.lib")   //windows下的socket库

#else
//Linux
#endif 

class Range
{
public:
	static bool GetRange(const std::string& range_str, int* start, int* end)
	{
		size_t  pos1 = range_str.find('-');
		size_t pos2 = range_str.find('=');
		*start = std::atol(range_str.substr(pos2 + 1, pos1 - pos2 - 1).c_str());
		*end = std::atol(range_str.substr(pos1 + 1).c_str());
		return true;
	}
};

class StringUtil
{
public:
	static int32_t Str2Dig(const std::string &num) //字符串转为数字
	{
		std::stringstream tmp;
		tmp << num; 
		int32_t res;
		tmp >> res; 

		return res; 
	}

};

class FileUtil
{
public:
	static int32_t GetFileSize(const std::string &name)
	{
		return boost::filesystem::file_size(name);
	}

	static bool Write(const std::string &name, const std::string &body, int32_t offset = 0) 
	{
		FILE *fp = NULL;
		fopen_s(&fp, name.c_str(), "wb+"); 
		if (fp == false)
		{
			std::cerr << "打开文件失败\n";
			return false;
		}
		fseek(fp, offset, SEEK_SET);
		size_t ret = fwrite(body.c_str(), 1, body.size(), fp);
		if (ret != body.size())
		{
			std::cerr << "向文件写入数据失败\n";
			fclose(fp);
			return false;
		}
		fclose(fp);
		return true;
	}
	static bool Read(const std::string &name, std::string *body)
	{
		int32_t filesize = boost::filesystem::file_size(name); //获取文件大小，通过文件名获取
		body->resize(filesize);//body分配空间就为文件大小

		FILE *fp = NULL;
		fopen_s(&fp, name.c_str(), "rb+");
		if (fp == NULL)
		{
			std::cerr << "打开文件失败\n";
			return false;
		}
		size_t ret = fread(&(*body)[0], 1, filesize, fp);
		if (ret != filesize)
		{
			std::cerr << "向文件读取数据失败\n";
			fclose(fp);
			return false;
		}
		fclose(fp);
		return true;
	}

	static bool ReadRange(const std::string &name, std::string *body, int32_t len, int32_t offset)//文件区间范围内数据读取
	{
		body->resize(len);
		FILE *fp = NULL;
		fopen_s(&fp, name.c_str(), "rb+");
		if (fp == NULL) 
		{
			std::cerr << "打开文件失败\n";
			return false;
		}
		fseek(fp, offset, SEEK_SET);
		size_t ret = fread(&(*body)[0], 1, len, fp);
		if (ret != len)
		{
			std::cerr << "向文件读取数据失败\n";
			fclose(fp);
			return false;
		}
		fclose(fp);
		return true;
	}
};

class Adapter
{
public:
	uint32_t _ip_addr;   //网卡上的IP地址
	uint32_t _mask_addr; //网卡上的子网掩码
};

class AdapterUtil //工具类网络适配器
{
public:

#ifdef _WIN32 
	static bool GetAllAdapter(std::vector<Adapter> *list)  //获取所有网卡信息，返回信息数组
	{
		PIP_ADAPTER_INFO p_adapters = new IP_ADAPTER_INFO(); //开辟一块网卡信息结构的空间 
		uint32_t all_adapter_size = sizeof(IP_ADAPTER_INFO); //获取网卡信息大小
		int ret = GetAdaptersInfo(p_adapters, (PULONG)&all_adapter_size); //all_adapter_size:用来获取实际所有网卡信息所占空间大小大小

		if (ret == ERROR_BUFFER_OVERFLOW) 
		{
			delete p_adapters; 
			p_adapters = (PIP_ADAPTER_INFO)new BYTE[all_adapter_size];   
			ret = GetAdaptersInfo(p_adapters, (PULONG)&all_adapter_size); 
		}

		while (p_adapters) 
		{
			Adapter adapter; 
			inet_pton(AF_INET, p_adapters->IpAddressList.IpAddress.String, &adapter._ip_addr); //inet_pton:将一个字符串点分十进制的IP地址转换成网络字节序IP地址
			inet_pton(AF_INET, p_adapters->IpAddressList.IpMask.String, &adapter._mask_addr);

			if (adapter._ip_addr != 0) //有网卡并没有启用
			{
				list->push_back(adapter); //将网卡信息添加到vector中返回给用户
			}
			p_adapters = p_adapters->Next; 
		}

		delete p_adapters; 
		return true;
	}

#else //Linux
#endif
};
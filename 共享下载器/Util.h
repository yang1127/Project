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
#include <Iphlpapi.h> //��ȡ������Ϣ�ӿڵ�ͷ�ļ�
#pragma comment(lib, "Iphlpapi.lib") //��ȡ������Ϣ�ӿڵĿ��ļ�����
#pragma comment(lib, "ws2_32.lib")   //windows�µ�socket��

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
	static int32_t Str2Dig(const std::string &num) //�ַ���תΪ����
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
			std::cerr << "���ļ�ʧ��\n";
			return false;
		}
		fseek(fp, offset, SEEK_SET);
		size_t ret = fwrite(body.c_str(), 1, body.size(), fp);
		if (ret != body.size())
		{
			std::cerr << "���ļ�д������ʧ��\n";
			fclose(fp);
			return false;
		}
		fclose(fp);
		return true;
	}
	static bool Read(const std::string &name, std::string *body)
	{
		int32_t filesize = boost::filesystem::file_size(name); //��ȡ�ļ���С��ͨ���ļ�����ȡ
		body->resize(filesize);//body����ռ��Ϊ�ļ���С

		FILE *fp = NULL;
		fopen_s(&fp, name.c_str(), "rb+");
		if (fp == NULL)
		{
			std::cerr << "���ļ�ʧ��\n";
			return false;
		}
		size_t ret = fread(&(*body)[0], 1, filesize, fp);
		if (ret != filesize)
		{
			std::cerr << "���ļ���ȡ����ʧ��\n";
			fclose(fp);
			return false;
		}
		fclose(fp);
		return true;
	}

	static bool ReadRange(const std::string &name, std::string *body, int32_t len, int32_t offset)//�ļ����䷶Χ�����ݶ�ȡ
	{
		body->resize(len);
		FILE *fp = NULL;
		fopen_s(&fp, name.c_str(), "rb+");
		if (fp == NULL) 
		{
			std::cerr << "���ļ�ʧ��\n";
			return false;
		}
		fseek(fp, offset, SEEK_SET);
		size_t ret = fread(&(*body)[0], 1, len, fp);
		if (ret != len)
		{
			std::cerr << "���ļ���ȡ����ʧ��\n";
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
	uint32_t _ip_addr;   //�����ϵ�IP��ַ
	uint32_t _mask_addr; //�����ϵ���������
};

class AdapterUtil //����������������
{
public:

#ifdef _WIN32 
	static bool GetAllAdapter(std::vector<Adapter> *list)  //��ȡ����������Ϣ��������Ϣ����
	{
		PIP_ADAPTER_INFO p_adapters = new IP_ADAPTER_INFO(); //����һ��������Ϣ�ṹ�Ŀռ� 
		uint32_t all_adapter_size = sizeof(IP_ADAPTER_INFO); //��ȡ������Ϣ��С
		int ret = GetAdaptersInfo(p_adapters, (PULONG)&all_adapter_size); //all_adapter_size:������ȡʵ������������Ϣ��ռ�ռ��С��С

		if (ret == ERROR_BUFFER_OVERFLOW) 
		{
			delete p_adapters; 
			p_adapters = (PIP_ADAPTER_INFO)new BYTE[all_adapter_size];   
			ret = GetAdaptersInfo(p_adapters, (PULONG)&all_adapter_size); 
		}

		while (p_adapters) 
		{
			Adapter adapter; 
			inet_pton(AF_INET, p_adapters->IpAddressList.IpAddress.String, &adapter._ip_addr); //inet_pton:��һ���ַ������ʮ���Ƶ�IP��ַת���������ֽ���IP��ַ
			inet_pton(AF_INET, p_adapters->IpAddressList.IpMask.String, &adapter._mask_addr);

			if (adapter._ip_addr != 0) //��������û������
			{
				list->push_back(adapter); //��������Ϣ��ӵ�vector�з��ظ��û�
			}
			p_adapters = p_adapters->Next; 
		}

		delete p_adapters; 
		return true;
	}

#else //Linux
#endif
};
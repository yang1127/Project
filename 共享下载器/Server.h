#pragma once
#include"Client.h"

class Server
{
public:
	bool Start()
	{
		//�����Կͻ�������Ĵ���ʽ��Ӧ��ϵ
		_srv.Get("/hostpair", HostPair);
		_srv.Get("/list", ShareList);
		_srv.Get("/download/.*", Download);
		_srv.listen("0.0.0.0", PKQ_PORT); 
		return true;
	}

private:
	static void HostPair(const httplib::Request &req, httplib::Response &rsp) 
	{
		rsp.status = 200; 
		return;
	}

	//��ȡ�����ļ��б�
	static void ShareList(const httplib::Request &req, httplib::Response &rsp)
	{
		if (!boost::filesystem::exists(SHARED_PATH))
		{
			boost::filesystem::create_directory(SHARED_PATH);
		}
		boost::filesystem::directory_iterator begin(SHARED_PATH); 
		boost::filesystem::directory_iterator end; 
		
		for (; begin != end; ++begin)
		{
			if (boost::filesystem::is_directory(begin->status())) 
			{
				continue;
			}
			
			std::string name = begin->path().filename().string(); 
			rsp.body += name + "\r\n";  //\r\n:���ļ��ָ�
		}
		rsp.status = 200; 
	}

	//�����ļ�
	static void Download(const httplib::Request &req, httplib::Response &rsp)
	{
		boost::filesystem::path req_path(req.path); 
		std::string name = req_path.filename().string(); 
		std::string realpath = SHARED_PATH + name; 

		if (!boost::filesystem::exists(realpath) || boost::filesystem::is_directory(realpath)) 
		{
			rsp.status = 404; 
			return;
		}

		if (req.method == "GET") //GET����
		{
			if (req.has_header("Range")) 
			{
				//Ϊ�ֿ鴫�䣬��Ҫ֪���ֿ������Ƕ���
				std::string range_str = req.get_header_value("Range"); 
				int32_t range_start = 0, range_end = 0, range_len = 0;
				Range::GetRange(range_str, &range_start, &range_end);
				range_len = range_end - range_start + 1;
				
				FileUtil::ReadRange(realpath, &rsp.body, range_len, range_start);
				rsp.status = 206;
			}
			else
			{
				if (FileUtil::Read(name, &rsp.body) == false) 
				{
					rsp.status = 500; 
				}

				rsp.status = 200; 
			}
		}
		else 
		{
			int32_t filesize = FileUtil::GetFileSize(realpath); //��ȡ�ļ���С
			rsp.set_header("Content-Length", std::to_string(filesize)); //������Ӧͷ���ӿ�
			rsp.status = 200;
		}
	}

private:

	httplib::Server _srv;
};
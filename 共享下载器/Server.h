#pragma once
#include"Client.h"

class Server
{
public:
	bool Start()
	{
		//添加针对客户端请求的处理方式对应关系
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

	//获取共享文件列表
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
			rsp.body += name + "\r\n";  //\r\n:做文件分隔
		}
		rsp.status = 200; 
	}

	//下载文件
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

		if (req.method == "GET") //GET请求
		{
			if (req.has_header("Range")) 
			{
				//为分块传输，需要知道分块区间是多少
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
			int32_t filesize = FileUtil::GetFileSize(realpath); //获取文件大小
			rsp.set_header("Content-Length", std::to_string(filesize)); //设置响应头部接口
			rsp.status = 200;
		}
	}

private:

	httplib::Server _srv;
};
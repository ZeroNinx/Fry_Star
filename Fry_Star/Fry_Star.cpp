#include "Fry_Star.h"

#define display(x) ui.pte_message->appendPlainText(x)
using namespace std;
using namespace boost::property_tree;
using namespace boost::beast::http;

//自定义：关闭套接字
void shutdown(boost::asio::ip::tcp::socket& socket)
{
	//优雅地关闭套接字
	boost::system::error_code ec;
	socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

	//检查错误
	if (ec && ec != boost::system::errc::not_connected)
		throw boost::system::system_error{ ec };
}

//构造函数、初始化
Fry_Star::Fry_Star(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	this->setFixedSize(width(), height());
	ui.pte_message->setReadOnly(true);
	
	//读取储存的cookie
	fstream fs;
	fs.open("cache.cfg", ios::in);
	if (fs)
	{
		string saved_cookie, line;
		while (!fs.eof())
		{
			fs >> line;
			saved_cookie += line;
		}
		ui.le_cookie->setText(qs(saved_cookie));
	}
	fs.close();

	//创建计时器
	timer = new QTimer(this);

	//绑定槽函数
	connect(timer, &QTimer::timeout, this, &Fry_Star::auto_sign);

}

//取得课程信息
bool Fry_Star::get_course()
{
	//创建套接字
	boost::asio::io_service io_se;
	boost::asio::ip::tcp::resolver resolver{ io_se };
	boost::asio::ip::tcp::socket socket{ io_se };

	try
	{
		//查找域名
		auto const results = resolver.resolve(api_host, port);
		// 链接找到的域名
		boost::asio::connect(socket, results.begin(), results.end());

		const string target = "/mycourse/backclazzdata?view=json&rss=1";
		//设定GET请求
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		//http头
		request_stream << "GET "<<target<<" HTTP/1.1\r\n";
		request_stream << "Host: " << api_host << "\r\n";
		request_stream << cookie << "\r\n";
		request_stream << ua << "\r\n\r\n";

		// 发送request
		write(socket, request);

		//声明一个容器来保存响应
		boost::beast::flat_buffer buf;
		response<string_body> resp;
		stringstream ss;

		//接收容器
		read(socket, buf, resp);
		ss << resp.body();
		//display(qs8(ss.str()));
		
		//分析课程
		ptree root;
		read_json(ss, root);
		if (root.get<int>("result") != 1)
		{
			display(qs("课程取得失败！"));
			return false;
		}

		BOOST_FOREACH(ptree::value_type & v, root.get_child("channelList"))
		{
			ptree content = v.second.get_child("content");
			if (content.find("course")!=content.not_found())
			{
				ptree data = content.get_child("course").get_child("data");
				int courseid, classid;
				string course_name, teacher;
				classid = content.get<int>("id");
				BOOST_FOREACH(ptree::value_type & d, data)
				{
					courseid = d.second.get<int>("id");
					course_name = d.second.get<string>("name");
					teacher = d.second.get<string>("teacherfactor");
					break;
				}
				course_list.push_back(Course(courseid, classid, course_name, teacher));
				ui.cb_unit->addItem(qs8(course_name + " " + teacher));
			}
		}
		shutdown(socket);
		return true;

	}
	catch (std::exception const& e)
	{
		//输出错误
		ui.pte_message->appendPlainText(qs((std::string)"错误：" + e.what()));
		ui.pte_message->appendPlainText(qs("好像潜入失败了......也许是神秘连接出错了？"));
		return false;
	}
}

//自动刷新签到
void Fry_Star::auto_sign()
{
	//创建套接字
	boost::asio::io_service io_se;
	boost::asio::ip::tcp::resolver resolver{ io_se };
	boost::asio::ip::tcp::socket socket{ io_se };
	try
	{
		//获取目标
		Course current_course = course_list[ui.cb_unit->currentIndex()];
		string target = "/ppt/activeAPI/taskactivelist?courseId=" + to_string(current_course.courseid) + "&classId=" + to_string(current_course.classid) + "&uid=" + uid;

		//查找域名
		auto const results = resolver.resolve(host, port);
		// 链接找到的域名
		boost::asio::connect(socket, results.begin(), results.end());

		//设定GET请求
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		//http头
		request_stream << "GET " << target << " HTTP/1.1\r\n";
		request_stream << "Host: " << host << "\r\n";
		request_stream << cookie << "\r\n";
		request_stream << ua << "\r\n\r\n";

		// 发送request
		write(socket, request);

		//声明一个容器来保存响应
		boost::beast::flat_buffer buf;
		response<string_body> resp;
		stringstream ss;

		//接收容器
		read(socket, buf, resp);
		ss << resp.body();
		//display(qs8(ss.str()));

		//分析JSON，寻找签到
		ptree root;
		read_json(ss, root);
		bool found_active = false;
		BOOST_FOREACH(ptree::value_type & v, root.get_child("activeList"))
		{
			if (v.second.find("nameTwo") != v.second.not_found())
			{
				if (v.second.get<int>("activeType") == 2 && v.second.get<int>("status") == 1)
				{
					string url = v.second.get<string>("url");
					map<string, string>mp = cut_string(url, '&');
					aid = mp["activePrimaryId"];
					if (aid_list[aid] == false)
					{
						//找到aid，签到
						found_active = true;
						sign();
					}
				}
			}
		}
		int siged = 0, unsiged = 0;
		for (auto i = aid_list.begin(); i != aid_list.end(); i++)
		{
			if (i->second)
				siged++;
			else unsiged++;
		}
		if (!found_active)
			display(qs("[爆破中][当前已签到："+to_string(siged)+", 未签到: "+to_string(unsiged))+qs("]未检测到签到活动......"));
		shutdown(socket);
	}
	catch (exception e)
	{
		display(qs("错误") + qs(e.what()));
	}
	
}

//签到
void Fry_Star::sign()
{
	//创建套接字
	boost::asio::io_service io_se;
	boost::asio::ip::tcp::resolver resolver{ io_se };
	boost::asio::ip::tcp::socket socket{ io_se };

	try
	{
		//获取目标
		string target = "/pptSign/stuSignajax?activeId=" + aid + "&uid=" + uid + "&clientip=&latitude=-1&longitude=-1&appType=15&fid=0";
		

		//查找域名
		auto const results = resolver.resolve(host, port);
		// 链接找到的域名
		boost::asio::connect(socket, results.begin(), results.end());

		//设定GET请求
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		//http头
		request_stream << "GET " << target << " HTTP/1.1\r\n";
		request_stream << "Host: " << host << "\r\n";
		request_stream << cookie << "\r\n";
		request_stream << ua << "\r\n\r\n";

		// 发送request
		write(socket, request);

		//声明一个容器来保存响应
		boost::beast::flat_buffer buf;
		response<string_body> resp;
		stringstream ss;

		//接收容器
		read(socket, buf, resp);
		if (resp.body() == "success")
		{
			display(qs("签到成功！"));
			aid_list[aid] = true;
		}
		else if (qs8(resp.body()) == qs("您已签到过了"))
		{
			aid_list[aid] = true;
		}
		else
		{
			display(qs("签到失败！"));
			display(qs("信息："));
			display(qs8(resp.body()));
		}
		shutdown(socket);
	}
	catch (exception e)
	{
		display(qs("错误") + qs(e.what()));
	}
	
}

//登录按钮
void Fry_Star::btn_submit_click()
{
	//检测cookie非空
	cookie = ui.le_cookie->text().toStdString();
	if (cookie.empty())
	{
		see(qs("请输入神秘Cookie！"));
		return;
	}

	//写入Cookie
	fstream fs;
	fs.open("cache.cfg",ios::out);
	if (fs)
		fs << ui.le_cookie->text().toStdString();
	fs.close();

	//开始执行任务
	ui.pte_message->appendPlainText(qs("正在潜入敌方总部......"));
	ui.pte_message->appendPlainText(qs("伪装UA：")+qs(ua));

	//解析出uid
	map<string, string> mp = cut_string(cookie, ';');
	for (auto i = mp.begin(); i != mp.end(); i++)
	{
		if (i->first == "UID")
			uid = i->second;
	}

	if (get_course() && !uid.empty())
		display(qs("潜入成功！请选择好要签到的课程，然后点击爆破吧！"));	
	else ui.pte_message->appendPlainText(qs("Cookie似乎出错了...？"));
}

//爆破按钮
void Fry_Star::btn_boom_click()
{
	display(qs("正在准备爆破......"));
	timer->setInterval(ui.sb_speed->value() * 1000);
	timer->start();
}

//单元选择切换
void Fry_Star::cb_unit_change(QString text)
{
	display(qs("当前科目：") + text);
}

//拆分字符串
std::map<std::string,std::string> Fry_Star::cut_string(std::string s,char c)
{
	map<string,string> mp;
	bool is_name = true;
	string name, value;
	ffor(i, 0, s.length()-1)
	{
		if (s[i] == c||s[i]=='?')
		{
			is_name = true;
			mp.insert(make_pair(name, value));
			name = "";
			value = "";
			continue;
		}

		if (s[i] != ' ')
		{
			if (s[i] == '=')
			{
				is_name = false;
				continue;
			}
			if (is_name)
				name.push_back(s[i]);
			else value.push_back(s[i]);
			
		}
	}
	mp.insert(make_pair(name, value));
	return mp;
}
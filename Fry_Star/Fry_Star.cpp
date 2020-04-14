#include "Fry_Star.h"
#include "sqlite3.h"


#define display(x) ui.pte_message->appendPlainText(x)
using namespace std;
using namespace boost::property_tree;
using namespace boost::beast::http;
using namespace CryptoPP;

string local_appdata_path = getenv("LOCALAPPDATA");

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
Fry_Star::Fry_Star(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	this->setFixedSize(width(), height());
	ui.pte_message->setReadOnly(true);

	//创建计时器
	timer = new QTimer(this);

	//设置任务栏小图标
	qti = new QSystemTrayIcon(this);
	qti->setIcon(QIcon("Resources/icon.ico"));
	connect(qti, &QSystemTrayIcon::activated, this, &Fry_Star::icon_active);
	
	QMenu* qti_menu= new QMenu;
	QAction* act_start = new QAction(qs("开始"), qti_menu);
	QAction* act_stop = new QAction(qs("暂停"), qti_menu);
	QAction* act_exit = new QAction(qs("退出"), qti_menu);
	connect(act_start, &QAction::triggered, this, &Fry_Star::btn_boom_click);
	connect(act_stop, &QAction::triggered, this, &Fry_Star::btn_stop_click);
	connect(act_exit, &QAction::triggered, this, &Fry_Star::icon_quit_click);
	qti_menu->addAction(act_start);
	qti_menu->addAction(act_stop);
	qti_menu->addAction(act_exit);
	qti->setContextMenu(qti_menu);
	qti->setToolTip(qs("监视已停止"));
	qti->show();

	//得到密钥
	DATA_BLOB DataOut;
	DATA_BLOB DataVerify;

	fstream fs;
	//读取储存的cookie
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

	//绑定槽函数
	connect(timer, &QTimer::timeout, this, &Fry_Star::auto_sign);

	ui.le_cookie->setText(qs("----------如果自动获取失败的话可以试试手动输入哦----------"));
	ui.pte_message->setFocus();
	auto_login();
}

//自定义：解密
string Fry_Star::decrypt(string s)
{
	DATA_BLOB DataOut;
	DATA_BLOB DataVerify;

	DataOut.pbData = (BYTE*)s.c_str();
	DataOut.cbData = s.size();

	//尝试Chrome80以下版本
	if (CryptUnprotectData(&DataOut, NULL, NULL, NULL, NULL, 0, &DataVerify))
	{
		string res;
		res.resize(DataVerify.cbData);
		ffor(i, 0, DataVerify.cbData - 1)
			res[i] = ((char*)DataVerify.pbData)[i];
		return res;
	}
	else//Chrome 80以上版本
	{
		ptree root;
		fstream fs;
		string file = local_appdata_path + "\\Google\\Chrome\\User Data\\Local State";
		fs.open(file, ios::in);
		if (fs)
		{
			read_json(fs, root);
			string encrypt_key = root.get_child("os_crypt").get<string>("encrypted_key");
			string enc_cache;
			StringSource ss1(encrypt_key, true, new Base64Decoder(new StringSink(enc_cache)));
			encrypt_key = enc_cache.substr(5, enc_cache.length() - 5);

			DataOut.pbData = (BYTE*)encrypt_key.c_str();
			DataOut.cbData = encrypt_key.size();
			CryptUnprotectData(&DataOut, NULL, NULL, NULL, NULL, 0, &DataVerify);
			key = (char*)DataVerify.pbData;//密钥
			key = key.substr(0, 32);
		}
		fs.close();
		if (s.substr(0,2)=="v1")
		{
			string nonce = s.substr(3, 12);//随机数
			string ciphertext = s.substr(3 + 12, s.length() - 16 - 3 - 12);//密文
			string tag = s.substr(s.length() - 16, 16);//身份验证标签
			string result;
			try
			{
				GCM<AES>::Decryption d;
				d.SetKeyWithIV((BYTE*)key.c_str(), key.size(), (BYTE*)nonce.c_str(), nonce.size());

				AuthenticatedDecryptionFilter df(d,
					new StringSink(result),
					AuthenticatedDecryptionFilter::MAC_AT_END, tag.size()
					);//设定认证模式

				StringSource ss2(ciphertext + tag, true,
					new Redirector(df)
					);//认证并解码

				if (true == df.GetLastResult()) 
					return result;
			}
			catch (CryptoPP::Exception& e)
			{
				display(qs(e.what()));
				return "err";
			}
			return "err";
		}
		return "err";
	}
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
		string tip = "监视中: " + toGBK(ui.cb_unit->currentText().toStdString()) + '\n' + "监视频率: " + to_string(ui.sb_speed->value()) + " 秒一次";
		qti->setToolTip(qs(tip));
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

//自动登录
void Fry_Star::auto_login()
{
	try
	{
		string file = local_appdata_path + "\\Google\\Chrome\\User Data\\Default\\Cookies";
		string sql = "select name,encrypted_value from cookies where host_key='.chaoxing.com';";

		sqlite3* sqlite = NULL;
		sqlite3_stmt* stmt = NULL;

		int res = sqlite3_open(file.c_str(), &sqlite);
		if (res != SQLITE_OK)
		{
			display(qs("读取失败辣！请记得留下Cookie！"));
			return;
		}
		res = sqlite3_prepare(sqlite, sql.c_str(), -1, &stmt, NULL);
		if (res != SQLITE_OK)
		{
			display(qs("数据库出错了！"));
			return;
		}

		//读取值并解密
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			string name, value;
			name = (char*)sqlite3_column_text(stmt, 0);
			int len = sqlite3_column_bytes(stmt, 1);
			value.resize(len);
			const void* blob = sqlite3_column_blob(stmt, 1);
			for (int i = 0; i < len; i++)
				value[i] = ((char*)blob)[i];
			value = decrypt(value);
			if (name == "UID")
				uid = value;
			if (value == "err")
			{
				display("异次元Cookie读取失败了！试试手动输入吧！");
				return;
			}
			cookie += name + "=" + value + ";";
		}
		ui.pte_message->appendPlainText(qs("你的曲奇已经准备好了！"));
		ui.pte_message->appendPlainText(qs("正在潜入敌方总部......"));
		ui.pte_message->appendPlainText(qs("伪装UA：") + qs(ua));
		if (get_course() && !uid.empty())
			display(qs("潜入成功！请选择好要签到的课程，然后点击爆破吧！"));
		else ui.pte_message->appendPlainText(qs("Cookie似乎出错了...？"));
	}
	catch (const std::exception&)
	{
		
	}
}

//手动登录按钮
void Fry_Star::btn_submit_click()
{
	if (ui.le_cookie->text().isEmpty())
	{
		display(qs("请输入神秘Cookie！"));
		return;
	}
	//写入Cookie
	fstream fs;
	fs.open("cache.cfg", ios::out);
	if (fs)
		fs << ui.le_cookie->text().toStdString();
	fs.close();
	cookie = ui.le_cookie->text().toStdString();

	//解析出uid
	map<string, string> mp = cut_string(cookie, ';');
	for (auto i = mp.begin(); i != mp.end(); i++)
	{
		if (i->first == "UID")
			uid = i->second;
	}

	ui.pte_message->setPlainText("");
	ui.cb_unit->clear();

	//开始执行任务
	ui.pte_message->appendPlainText(qs("正在潜入敌方总部......"));
	ui.pte_message->appendPlainText(qs("伪装UA：") + qs(ua));
	if (get_course() && !uid.empty())
		display(qs("潜入成功！请选择好要签到的课程，然后点击爆破吧！"));
	else ui.pte_message->appendPlainText(qs("Cookie似乎出错了...？"));
}

//爆破按钮
void Fry_Star::btn_boom_click()
{
	if (ui.cb_unit->currentText().isEmpty())
		return;
	string tip = "监视中: " + toGBK(ui.cb_unit->currentText().toStdString()) + '\n' + "监视频率: " + to_string(ui.sb_speed->value()) + " 秒一次";
	qti->setToolTip(qs(tip));
	display(qs("正在准备爆破......"));
	timer->setInterval(ui.sb_speed->value() * 1000);
	timer->start();
}

//暂停按钮
void Fry_Star::btn_stop_click()
{
	timer->stop();
	display(qs("运动结束，放松一下吧~"));
	string tip = "监视已停止: " + toGBK(ui.cb_unit->currentText().toStdString());
	qti->setToolTip(qs(tip));
}

//单元选择切换
void Fry_Star::cb_unit_change(QString text)
{
	if (!text.isEmpty())
		display(qs("当前科目：") + text);
}

//从图标唤醒
void Fry_Star::icon_active(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::DoubleClick)
	{
		this->show();
		this->setFocus();
	}
}

//从图标关闭
void Fry_Star::icon_quit_click()
{
	icon_quit = true;
	this->close();
}

//关闭事件
void Fry_Star::closeEvent(QCloseEvent* event)
{
	this->hide();
	if (!icon_quit && timer->isActive())
	{
		qti->showMessage(qs("已最小化"), qs("将在后台持续检测签到"));
		event->ignore();
	}
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

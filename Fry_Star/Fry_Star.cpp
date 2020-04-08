#include "Fry_Star.h"

#define display(x) ui.pte_message->appendPlainText(x)
using namespace std;
using namespace boost::property_tree;
using namespace boost::beast::http;

//�Զ��壺�ر��׽���
void shutdown(boost::asio::ip::tcp::socket& socket)
{
	//���ŵعر��׽���
	boost::system::error_code ec;
	socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

	//������
	if (ec && ec != boost::system::errc::not_connected)
		throw boost::system::system_error{ ec };
}

//���캯������ʼ��
Fry_Star::Fry_Star(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	this->setFixedSize(width(), height());
	ui.pte_message->setReadOnly(true);
	
	//��ȡ�����cookie
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

	//������ʱ��
	timer = new QTimer(this);

	//�󶨲ۺ���
	connect(timer, &QTimer::timeout, this, &Fry_Star::auto_sign);

}

//ȡ�ÿγ���Ϣ
bool Fry_Star::get_course()
{
	//�����׽���
	boost::asio::io_service io_se;
	boost::asio::ip::tcp::resolver resolver{ io_se };
	boost::asio::ip::tcp::socket socket{ io_se };

	try
	{
		//��������
		auto const results = resolver.resolve(api_host, port);
		// �����ҵ�������
		boost::asio::connect(socket, results.begin(), results.end());

		const string target = "/mycourse/backclazzdata?view=json&rss=1";
		//�趨GET����
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		//httpͷ
		request_stream << "GET "<<target<<" HTTP/1.1\r\n";
		request_stream << "Host: " << api_host << "\r\n";
		request_stream << cookie << "\r\n";
		request_stream << ua << "\r\n\r\n";

		// ����request
		write(socket, request);

		//����һ��������������Ӧ
		boost::beast::flat_buffer buf;
		response<string_body> resp;
		stringstream ss;

		//��������
		read(socket, buf, resp);
		ss << resp.body();
		//display(qs8(ss.str()));
		
		//�����γ�
		ptree root;
		read_json(ss, root);
		if (root.get<int>("result") != 1)
		{
			display(qs("�γ�ȡ��ʧ�ܣ�"));
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
		//�������
		ui.pte_message->appendPlainText(qs((std::string)"����" + e.what()));
		ui.pte_message->appendPlainText(qs("����Ǳ��ʧ����......Ҳ�����������ӳ����ˣ�"));
		return false;
	}
}

//�Զ�ˢ��ǩ��
void Fry_Star::auto_sign()
{
	//�����׽���
	boost::asio::io_service io_se;
	boost::asio::ip::tcp::resolver resolver{ io_se };
	boost::asio::ip::tcp::socket socket{ io_se };
	try
	{
		//��ȡĿ��
		Course current_course = course_list[ui.cb_unit->currentIndex()];
		string target = "/ppt/activeAPI/taskactivelist?courseId=" + to_string(current_course.courseid) + "&classId=" + to_string(current_course.classid) + "&uid=" + uid;

		//��������
		auto const results = resolver.resolve(host, port);
		// �����ҵ�������
		boost::asio::connect(socket, results.begin(), results.end());

		//�趨GET����
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		//httpͷ
		request_stream << "GET " << target << " HTTP/1.1\r\n";
		request_stream << "Host: " << host << "\r\n";
		request_stream << cookie << "\r\n";
		request_stream << ua << "\r\n\r\n";

		// ����request
		write(socket, request);

		//����һ��������������Ӧ
		boost::beast::flat_buffer buf;
		response<string_body> resp;
		stringstream ss;

		//��������
		read(socket, buf, resp);
		ss << resp.body();
		//display(qs8(ss.str()));

		//����JSON��Ѱ��ǩ��
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
						//�ҵ�aid��ǩ��
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
			display(qs("[������][��ǰ��ǩ����"+to_string(siged)+", δǩ��: "+to_string(unsiged))+qs("]δ��⵽ǩ���......"));
		shutdown(socket);
	}
	catch (exception e)
	{
		display(qs("����") + qs(e.what()));
	}
	
}

//ǩ��
void Fry_Star::sign()
{
	//�����׽���
	boost::asio::io_service io_se;
	boost::asio::ip::tcp::resolver resolver{ io_se };
	boost::asio::ip::tcp::socket socket{ io_se };

	try
	{
		//��ȡĿ��
		string target = "/pptSign/stuSignajax?activeId=" + aid + "&uid=" + uid + "&clientip=&latitude=-1&longitude=-1&appType=15&fid=0";
		

		//��������
		auto const results = resolver.resolve(host, port);
		// �����ҵ�������
		boost::asio::connect(socket, results.begin(), results.end());

		//�趨GET����
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		//httpͷ
		request_stream << "GET " << target << " HTTP/1.1\r\n";
		request_stream << "Host: " << host << "\r\n";
		request_stream << cookie << "\r\n";
		request_stream << ua << "\r\n\r\n";

		// ����request
		write(socket, request);

		//����һ��������������Ӧ
		boost::beast::flat_buffer buf;
		response<string_body> resp;
		stringstream ss;

		//��������
		read(socket, buf, resp);
		if (resp.body() == "success")
		{
			display(qs("ǩ���ɹ���"));
			aid_list[aid] = true;
		}
		else if (qs8(resp.body()) == qs("����ǩ������"))
		{
			aid_list[aid] = true;
		}
		else
		{
			display(qs("ǩ��ʧ�ܣ�"));
			display(qs("��Ϣ��"));
			display(qs8(resp.body()));
		}
		shutdown(socket);
	}
	catch (exception e)
	{
		display(qs("����") + qs(e.what()));
	}
	
}

//��¼��ť
void Fry_Star::btn_submit_click()
{
	//���cookie�ǿ�
	cookie = ui.le_cookie->text().toStdString();
	if (cookie.empty())
	{
		see(qs("����������Cookie��"));
		return;
	}

	//д��Cookie
	fstream fs;
	fs.open("cache.cfg",ios::out);
	if (fs)
		fs << ui.le_cookie->text().toStdString();
	fs.close();

	//��ʼִ������
	ui.pte_message->appendPlainText(qs("����Ǳ��з��ܲ�......"));
	ui.pte_message->appendPlainText(qs("αװUA��")+qs(ua));

	//������uid
	map<string, string> mp = cut_string(cookie, ';');
	for (auto i = mp.begin(); i != mp.end(); i++)
	{
		if (i->first == "UID")
			uid = i->second;
	}

	if (get_course() && !uid.empty())
		display(qs("Ǳ��ɹ�����ѡ���Ҫǩ���Ŀγ̣�Ȼ�������ưɣ�"));	
	else ui.pte_message->appendPlainText(qs("Cookie�ƺ�������...��"));
}

//���ư�ť
void Fry_Star::btn_boom_click()
{
	display(qs("����׼������......"));
	timer->setInterval(ui.sb_speed->value() * 1000);
	timer->start();
}

//��Ԫѡ���л�
void Fry_Star::cb_unit_change(QString text)
{
	display(qs("��ǰ��Ŀ��") + text);
}

//����ַ���
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
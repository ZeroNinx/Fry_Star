#include "Fry_Star.h"
#include "sqlite3.h"


#define display(x) ui.pte_message->appendPlainText(x)
using namespace std;
using namespace boost::property_tree;
using namespace boost::beast::http;
using namespace CryptoPP;

string local_appdata_path = getenv("LOCALAPPDATA");

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
Fry_Star::Fry_Star(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	this->setFixedSize(width(), height());
	ui.pte_message->setReadOnly(true);

	//������ʱ��
	timer = new QTimer(this);

	//����������Сͼ��
	qti = new QSystemTrayIcon(this);
	qti->setIcon(QIcon("Resources/icon.ico"));
	connect(qti, &QSystemTrayIcon::activated, this, &Fry_Star::icon_active);
	
	QMenu* qti_menu= new QMenu;
	QAction* act_start = new QAction(qs("��ʼ"), qti_menu);
	QAction* act_stop = new QAction(qs("��ͣ"), qti_menu);
	QAction* act_exit = new QAction(qs("�˳�"), qti_menu);
	connect(act_start, &QAction::triggered, this, &Fry_Star::btn_boom_click);
	connect(act_stop, &QAction::triggered, this, &Fry_Star::btn_stop_click);
	connect(act_exit, &QAction::triggered, this, &Fry_Star::icon_quit_click);
	qti_menu->addAction(act_start);
	qti_menu->addAction(act_stop);
	qti_menu->addAction(act_exit);
	qti->setContextMenu(qti_menu);
	qti->setToolTip(qs("������ֹͣ"));
	qti->show();

	//�õ���Կ
	DATA_BLOB DataOut;
	DATA_BLOB DataVerify;

	fstream fs;
	//��ȡ�����cookie
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

	//�󶨲ۺ���
	connect(timer, &QTimer::timeout, this, &Fry_Star::auto_sign);

	ui.le_cookie->setText(qs("----------����Զ���ȡʧ�ܵĻ����������ֶ�����Ŷ----------"));
	ui.pte_message->setFocus();
	auto_login();
}

//�Զ��壺����
string Fry_Star::decrypt(string s)
{
	DATA_BLOB DataOut;
	DATA_BLOB DataVerify;

	DataOut.pbData = (BYTE*)s.c_str();
	DataOut.cbData = s.size();

	//����Chrome80���°汾
	if (CryptUnprotectData(&DataOut, NULL, NULL, NULL, NULL, 0, &DataVerify))
	{
		string res;
		res.resize(DataVerify.cbData);
		ffor(i, 0, DataVerify.cbData - 1)
			res[i] = ((char*)DataVerify.pbData)[i];
		return res;
	}
	else//Chrome 80���ϰ汾
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
			key = (char*)DataVerify.pbData;//��Կ
			key = key.substr(0, 32);
		}
		fs.close();
		if (s.substr(0,2)=="v1")
		{
			string nonce = s.substr(3, 12);//�����
			string ciphertext = s.substr(3 + 12, s.length() - 16 - 3 - 12);//����
			string tag = s.substr(s.length() - 16, 16);//�����֤��ǩ
			string result;
			try
			{
				GCM<AES>::Decryption d;
				d.SetKeyWithIV((BYTE*)key.c_str(), key.size(), (BYTE*)nonce.c_str(), nonce.size());

				AuthenticatedDecryptionFilter df(d,
					new StringSink(result),
					AuthenticatedDecryptionFilter::MAC_AT_END, tag.size()
					);//�趨��֤ģʽ

				StringSource ss2(ciphertext + tag, true,
					new Redirector(df)
					);//��֤������

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
		string tip = "������: " + toGBK(ui.cb_unit->currentText().toStdString()) + '\n' + "����Ƶ��: " + to_string(ui.sb_speed->value()) + " ��һ��";
		qti->setToolTip(qs(tip));
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

//�Զ���¼
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
			display(qs("��ȡʧ��������ǵ�����Cookie��"));
			return;
		}
		res = sqlite3_prepare(sqlite, sql.c_str(), -1, &stmt, NULL);
		if (res != SQLITE_OK)
		{
			display(qs("���ݿ�����ˣ�"));
			return;
		}

		//��ȡֵ������
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
				display("���ԪCookie��ȡʧ���ˣ������ֶ�����ɣ�");
				return;
			}
			cookie += name + "=" + value + ";";
		}
		ui.pte_message->appendPlainText(qs("��������Ѿ�׼�����ˣ�"));
		ui.pte_message->appendPlainText(qs("����Ǳ��з��ܲ�......"));
		ui.pte_message->appendPlainText(qs("αװUA��") + qs(ua));
		if (get_course() && !uid.empty())
			display(qs("Ǳ��ɹ�����ѡ���Ҫǩ���Ŀγ̣�Ȼ�������ưɣ�"));
		else ui.pte_message->appendPlainText(qs("Cookie�ƺ�������...��"));
	}
	catch (const std::exception&)
	{
		
	}
}

//�ֶ���¼��ť
void Fry_Star::btn_submit_click()
{
	if (ui.le_cookie->text().isEmpty())
	{
		display(qs("����������Cookie��"));
		return;
	}
	//д��Cookie
	fstream fs;
	fs.open("cache.cfg", ios::out);
	if (fs)
		fs << ui.le_cookie->text().toStdString();
	fs.close();
	cookie = ui.le_cookie->text().toStdString();

	//������uid
	map<string, string> mp = cut_string(cookie, ';');
	for (auto i = mp.begin(); i != mp.end(); i++)
	{
		if (i->first == "UID")
			uid = i->second;
	}

	ui.pte_message->setPlainText("");
	ui.cb_unit->clear();

	//��ʼִ������
	ui.pte_message->appendPlainText(qs("����Ǳ��з��ܲ�......"));
	ui.pte_message->appendPlainText(qs("αװUA��") + qs(ua));
	if (get_course() && !uid.empty())
		display(qs("Ǳ��ɹ�����ѡ���Ҫǩ���Ŀγ̣�Ȼ�������ưɣ�"));
	else ui.pte_message->appendPlainText(qs("Cookie�ƺ�������...��"));
}

//���ư�ť
void Fry_Star::btn_boom_click()
{
	if (ui.cb_unit->currentText().isEmpty())
		return;
	string tip = "������: " + toGBK(ui.cb_unit->currentText().toStdString()) + '\n' + "����Ƶ��: " + to_string(ui.sb_speed->value()) + " ��һ��";
	qti->setToolTip(qs(tip));
	display(qs("����׼������......"));
	timer->setInterval(ui.sb_speed->value() * 1000);
	timer->start();
}

//��ͣ��ť
void Fry_Star::btn_stop_click()
{
	timer->stop();
	display(qs("�˶�����������һ�°�~"));
	string tip = "������ֹͣ: " + toGBK(ui.cb_unit->currentText().toStdString());
	qti->setToolTip(qs(tip));
}

//��Ԫѡ���л�
void Fry_Star::cb_unit_change(QString text)
{
	if (!text.isEmpty())
		display(qs("��ǰ��Ŀ��") + text);
}

//��ͼ�껽��
void Fry_Star::icon_active(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::DoubleClick)
	{
		this->show();
		this->setFocus();
	}
}

//��ͼ��ر�
void Fry_Star::icon_quit_click()
{
	icon_quit = true;
	this->close();
}

//�ر��¼�
void Fry_Star::closeEvent(QCloseEvent* event)
{
	this->hide();
	if (!icon_quit && timer->isActive())
	{
		qti->showMessage(qs("����С��"), qs("���ں�̨�������ǩ��"));
		event->ignore();
	}
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

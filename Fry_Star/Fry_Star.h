#pragma once

#include "stdafx.h"
#include "ui_Fry_Star.h"
#include "course.h"

class Fry_Star : public QWidget
{
	Q_OBJECT

public:
	Fry_Star(QWidget *parent = Q_NULLPTR);
	QTimer* timer;

	std::vector<Course> course_list;//课程列表

	const std::string api_host = "mooc1-api.chaoxing.com";
	const std::string host = "mobilelearn.chaoxing.com";
	const std::string port = "80";	
	const std::string ua = "User-Agent: Mozilla/5.0 (iPad; CPU OS 13_3_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Mobile/15E148 ChaoXingStudy/ChaoXingStudy_3_4.3.2_ios_phone_201911291130_27 (@Kalimdor)_11391565702936108810"; //浏览器标识
	
	std::string key;
	std::string cookie="Cookie:";
	std::string uid;
	std::string aid;

	//保存活动列表
	std::map<std::string, bool> aid_list;

private:
	Ui::MainForm ui;
	bool get_course();
	void sign();
	void auto_login();
	std::string decrypt(std::string s);
	std::map<std::string, std::string> cut_string(std::string, char);

public slots:
	void auto_sign();
	void btn_boom_click();
	void btn_submit_click();
	void cb_unit_change(QString);
};

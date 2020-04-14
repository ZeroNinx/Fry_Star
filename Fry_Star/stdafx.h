#pragma once
#pragma once
#include <sdkddkver.h>

//std
#include <fstream>
#include <iostream>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

//Boost
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/beast.hpp>
#include <boost/locale.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//WinAPI
#include <Windows.h>
#include <wincrypt.h>

//Qt
#include <QAction>
#include <QCloseEvent>
#include <QMenu>
#include <QString>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QtWidgets/QApplication>
#include <QMessageBox>

//Cryptopp
#include <cryptopp/aes.h>
#include <cryptopp/base64.h>
#include <cryptopp/filters.h>
#include <cryptopp/gcm.h>

#define see(x) QMessageBox::information(NULL, qs("提示"), x)
#define ffor(i,a,b) for(int i=a;i<=b;i++)
#define rfor(i,a,b) for(int i=a;i>=b;i--)

//快速转QString
QString qs(std::string text);
QString qs(char* text);
QString qs8(std::string text);

//编码转换
std::string toGBK(const std::string & str);

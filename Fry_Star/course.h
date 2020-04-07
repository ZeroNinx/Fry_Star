#pragma once
#include "stdafx.h"

class Course
{
public:
	Course(int cour, int clas, std::string nam, std::string tech) :courseid(cour), classid(clas), name(nam), teacher(tech) {};
	int courseid;
	int classid;
	std::string name;
	std::string teacher;
};


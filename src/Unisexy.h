#pragma once

#include <ClibUtil/singleton.hpp>

class Unisexy : public clib_util::singleton::ISingleton<Unisexy>
{
public:
	// Main processing function - creates gender-flipped versions of head parts
	// based on configuration settings loaded from Unisexy.ini
	void DoSexyStuff();
};

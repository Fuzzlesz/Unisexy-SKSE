#pragma once

#include <ClibUtil/singleton.hpp>

class Unisexy : public clib_util::singleton::ISingleton<Unisexy>
{
public:
	void DoSexyStuff();
};

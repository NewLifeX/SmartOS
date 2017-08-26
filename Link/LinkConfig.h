#ifndef __LinkConfig_H__
#define __LinkConfig_H__

#include "..\Config.h"
#include "Net\NetUri.h"

// 配置信息
class LinkConfig : public ConfigBase
{
public:
	byte	Length;			// 数据长度

	char	_User[16];		// 登录名
	char	_Pass[16];		// 登录密码
	char	_Server[32];	// 服务器

	byte	TagEnd;		// 数据区结束标识符

	LinkConfig();
	virtual void Init();
	virtual void Show() const;

	String	User()		{ return String(_User, sizeof(_User), false); }
	String	Pass()		{ return String(_Pass, sizeof(_Pass), false); }
	String	Server()	{ return String(_Server, sizeof(_Server), false); }
	NetUri	Uri()		{ return NetUri(Server()); }

	static LinkConfig* Current;
	static LinkConfig*	Create(cstring server);
};

#endif

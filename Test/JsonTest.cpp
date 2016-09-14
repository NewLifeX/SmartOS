#include "Sys.h"
#include "Message\Json.h"

#if DEBUG
static void TestRead()
{
	Json json	=
"{\
	\"id\": 3141,			\
	\"name\": \"Stone\",	\
	\"enable\": true,		\
	\"noval\": null,		\
	\"score\": 3.141,		\
	\"array\": [1, 0, 2],	\
	\"extend\": {			\
		\"kind\": \"cost\",	\
		\"value\": 42.99	\
	}\
}";

	assert(json.Type() == JsonType::object, "Type()");

	auto id		= json["id"];
	assert(id.Type() == JsonType::integer, "Type()");

	auto name	= json["name"];
	assert(name.Type() == JsonType::string, "Type()");

	auto enable	= json["enable"];
	assert(id.Type() == JsonType::boolean, "Type()");

	auto noval	= json["noval"];
	assert(noval.Type() == JsonType::null, "Type()");

	auto score	= json["score"];
	assert(score.Type() == JsonType::Float, "Type()");

	auto array	= json["array"];
	assert(array.Type() == JsonType::array, "Type()");

	auto extend	= json["extend"];
	assert(extend.Type() == JsonType::object, "Type()");
}

void Json::Test()
{
	TS("TestList");

	debug_printf("TestJson......\r\n");

	TestRead();

	debug_printf("TestJson 测试完毕......\r\n");

}
#endif

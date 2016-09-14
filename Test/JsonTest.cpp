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
	\"score\": 3.1415,		\
	\"array\": [1, 0, 2],	\
	\"extend\": {			\
		\"kind\": \"cost\",	\
		\"value\": 67.89	\
	}\
}";

	assert(json.Type() == JsonType::object, "Type()");

	auto id		= json["id"];
	assert(id.Type() == JsonType::integer, "Type()");
	assert(id.AsInt() == 3141, "AsInt()");

	auto name	= json["name"];
	assert(name.Type() == JsonType::string, "Type()");
	assert(name.AsString() == "Stone", "AsString()");

	auto enable	= json["enable"];
	assert(enable.Type() == JsonType::boolean, "Type()");
	assert(enable.AsBoolean() == true, "AsBoolean()");

	auto noval	= json["noval"];
	assert(noval.Type() == JsonType::null, "Type()");

	auto score	= json["score"];
	assert(score.Type() == JsonType::Float, "Type()");
	assert(score.AsFloat() == 3.1415, "AsFloat()");

	auto array	= json["array"];
	assert(array.Type() == JsonType::array, "Type()");
	assert(score.Length() == 3, "Length()");

	auto extend	= json["extend"];
	assert(extend.Type() == JsonType::object, "Type()");

	auto kind	= extend["kind"];
	assert(kind.Type() == JsonType::string, "Type()");
	assert(kind.AsString() == "cost", "AsString()");

	auto value	= extend["value"];
	assert(value.Type() == JsonType::Float, "Type()");
	assert(value.AsFloat() == 67.89, "AsFloat()");
}

void Json::Test()
{
	TS("TestJson");

	debug_printf("TestJson......\r\n");

	TestRead();

	debug_printf("TestJson 测试完毕......\r\n");

}
#endif

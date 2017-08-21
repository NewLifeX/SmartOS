#include "Kernel\Sys.h"
#include "Message\Json.h"

#if DEBUG
static cstring jsonstr =
"{\
	\"id\": 3141,			\
	\"name\": \"Smart \\\" Stone\",	\
	\"enable\": true,		\
	\"noval\": null,		\
	\"score\": 3.14159,		\
	\"array\": [1, 0, 2],	\
	\"extend\": {			\
		\"kind\": \"cost\",	\
		\"value\": 67.89	\
	}\
}";

static void TestRead()
{
	Json json = jsonstr;

	assert(json.Type() == JsonType::object, "Type()");

	auto id = json["id"];
	debug_printf("id=");	id.Show(true);
	assert(id.Type() == JsonType::integer, "Type()");
	assert(id.AsInt() == 3141, "AsInt()");

	auto name = json["name"];
	debug_printf("name=");	name.Show(true);
	assert(name.Type() == JsonType::string, "Type()");
	assert(name.AsString() == "Smart \\\" Stone", "AsString()");

	auto enable = json["enable"];
	debug_printf("enable=");	enable.Show(true);
	assert(enable.Type() == JsonType::boolean, "Type()");
	assert(enable.AsBoolean() == true, "AsBoolean()");

	auto noval = json["noval"];
	debug_printf("noval=");	noval.Show(true);
	assert(noval.Type() == JsonType::null, "Type()");

	auto score = json["score"];
	debug_printf("score=");	score.Show(true);
	assert(score.Type() == JsonType::Float, "Type()");
	float v = score.AsFloat();
	String s(v);
	s.Show(true);
	double v2 = score.AsDouble();
	//double v2	= 3.1415;
	String s2(v2);
	s2.Show(true);
	assert(score.AsDouble() == 3.14159, "AsFloat()");

	auto array = json["array"];
	debug_printf("array=");	array.Show(true);
	assert(array.Type() == JsonType::array, "Type()");
	assert(array.Length() == 3, "Length()");

	auto arr2 = array[2];
	debug_printf("array[2]=");	arr2.Show(true);
	assert(arr2.Type() == JsonType::integer, "Type()");
	assert(arr2.AsInt() == 2, "AsInt()");

	auto extend = json["extend"];
	debug_printf("extend=");	extend.Show(true);
	assert(extend.Type() == JsonType::object, "Type()");

	auto kind = extend["kind"];
	debug_printf("kind=");	kind.Show(true);
	assert(kind.Type() == JsonType::string, "Type()");
	assert(kind.AsString() == "cost", "AsString()");

	auto value = extend["value"];
	debug_printf("value=");	value.Show(true);
	assert(value.Type() == JsonType::Float, "Type()");
	float v3 = value.AsFloat();
	String s3(v3);
	s3.Show(true);
	//assert(value.AsFloat() == 67.89, "AsFloat()");
	// 必须加上f结尾，说明这是单精度浮点数，否则不想等
	assert(value.AsFloat() == 67.89f, "AsFloat()");
}

static void TestWrite()
{
	Json json;

	/*json["id"]		= 3141;
	json["name"]	= "Smart \" Stone";
	json["enable"]	= "true";
	json["noval"]	= nullptr;
	json["score"]	= 3.14159;*/
	json.Add("id", 3141);
	json.Add("name", "Smart \\\" Stone");
	json.Add("enable", "true");
	json.Add("noval", nullptr);
	json.Add("score", 3.14159);

	Json arr;
	/*arr[0]	= 1;
	arr[1]	= 0;
	arr[2]	= 2;*/
	arr.Add(1).Add(0).Add(2);
	json.Add("array", arr);

	Json ext;
	/*ext["kind"]	= "cost";
	ext["value"]= 67.89f;*/
	ext.Add("kind", "cost");
	ext.Add("value", 67.89f);
	json.Add("extend", ext);

	//auto rs	= json.ToString();
	json.Show(true);
	//assert(rs == jsonstr, "ToString()");
}

void Json::Test()
{
	TS("TestJson");

	debug_printf("TestJson......\r\n");

	TestRead();
	TestWrite();

	debug_printf("TestJson 测试完毕......\r\n");

}
#endif

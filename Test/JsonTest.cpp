#include "Kernel\Sys.h"
#include "Message\Json.h"

#if DEBUG
static cstring jsonstr	=
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
	Json json	= jsonstr;

	assert(json.Type() == JsonType::object, "Type()");

	auto id		= json["id"];
	assert(id.Type() == JsonType::integer, "Type()");
	assert(id.AsInt() == 3141, "AsInt()");

	auto name	= json["name"];
	assert(name.Type() == JsonType::string, "Type()");
	assert(name.AsString() == "Smart \\\" Stone", "AsString()");

	auto enable	= json["enable"];
	assert(enable.Type() == JsonType::boolean, "Type()");
	assert(enable.AsBoolean() == true, "AsBoolean()");

	auto noval	= json["noval"];
	assert(noval.Type() == JsonType::null, "Type()");

	auto score	= json["score"];
	assert(score.Type() == JsonType::Float, "Type()");
	float v	= score.AsFloat();
	String s(v);
	s.Show(true);
	double v2	= score.AsDouble();
	//double v2	= 3.1415;
	String s2(v2);
	s2.Show(true);
	assert(score.AsDouble() == 3.14159, "AsFloat()");

	auto array	= json["array"];
	assert(array.Type() == JsonType::array, "Type()");
	assert(array.Length() == 3, "Length()");

	auto arr2	= array[2];
	assert(arr2.Type() == JsonType::integer, "Type()");
	assert(arr2.AsInt() == 2, "AsInt()");

	auto extend	= json["extend"];
	assert(extend.Type() == JsonType::object, "Type()");

	auto kind	= extend["kind"];
	assert(kind.Type() == JsonType::string, "Type()");
	assert(kind.AsString() == "cost", "AsString()");

	auto value	= extend["value"];
	assert(value.Type() == JsonType::Float, "Type()");
	float v3	= value.AsFloat();
	String s3(v3);
	s3.Show(true);
	//assert(value.AsFloat() == 67.89, "AsFloat()");
	// 必须加上f结尾，说明这是单精度浮点数，否则不想等
	assert(value.AsFloat() == 67.89f, "AsFloat()");
}

static void TestWrite()
{
	String rs;
	Json json(rs);
	//json.SetOut(rs);

	/*json["id"]		= 3141;
	json["name"]	= "Smart \" Stone";
	json["enable"]	= "true";
	json["noval"]	= nullptr;
	json["score"]	= 3.14159;*/
	json.Add("id",		3141);
	json.Add("name",	"Smart \" Stone");
	json.Add("enable",	"true");
	json.Add("noval",	nullptr);
	json.Add("score",	3.14159);

	auto arr	= json.AddArray("array");
	/*arr[0]	= 1;
	arr[1]	= 0;
	arr[2]	= 2;*/
	arr.Add(1).Add(0).Add(2);

	auto ext	= json["extend"];
	/*ext["kind"]	= "cost";
	ext["value"]= 67.89f;*/
	ext.Add("kind",		"cost");
	ext.Add("value",	67.89f);

	//auto rs	= json.ToString();
	rs.Show(true);
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

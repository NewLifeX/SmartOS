#include "Json.h"

static bool isSpace(char ch);
static cstring SkipSpace(cstring str, int& len);
static int find(cstring str, int len, char ch);

static const Json Null(nullptr);

// 构造只读实例
Json::Json(cstring str) :_str(str) { }
Json::Json(cstring str, int len) : _str(str, len) { }
Json::Json(const String& value) : _str(value) { }

// 值类型
JsonType Json::Type() const
{
	if (!_str) return JsonType::null;

	// 快速判断对象、数组和字符串
	auto s = _str.Trim();
	auto p = s.GetBuffer();
	int len = s.Length();
	switch (s[0])
	{
	case '{': return JsonType::object;
	case '[': return JsonType::array;
	case '"': return JsonType::string;
	}

	// 对比判断空和布尔类型
	if (s == "null") return JsonType::null;
	if (s == "true" || s == "false") return JsonType::boolean;

	// 剩下整数和浮点数
	bool isFloat = false;
	for (int i = 0; i < len; i++)
	{
		char ch = s[i];
		// 判断非数字
		if (ch < '0' || ch > '9')
		{
			// 负号只能出现在第一位
			if (ch == '-' && i > 0) return JsonType::null;

			// 直接判断圆点，不支持指数类型
			if (ch == '.' && !isFloat)
				isFloat = true;
			else
				return JsonType::null;
		}
	}

	return isFloat ? JsonType::Float : JsonType::integer;
}

bool Json::IsNull() const { return !_str; }

// 获取值
String Json::AsString() const {
	if (!_str) return nullptr;

	if (_str[0] != '"') return nullptr;

	// 去掉前后双引号
	auto p = _str.GetBuffer();
	int n = _str.Length();
	if (p[0] == '"')
	{
		p++;
		n--;
	}
	if (p[n - 1] == '"') n--;

	// 没有处理转义字符
	return String(p, n);
}

bool Json::AsBoolean() const {
	if (!_str) return false;

	//if (_str[0] != 't' && _str[0] != 'f') return false;

	return _str.Trim() == "true";
}

int Json::AsInt() const {
	if (!_str) return 0;

	if (Type() != JsonType::integer) return 0;

	return _str.Trim().ToInt();
}

float Json::AsFloat() const {
	if (!_str) return 0;

	if (Type() != JsonType::Float) return 0;

	return _str.Trim().ToFloat();
}

double Json::AsDouble() const
{
	if (!_str) return 0;

	if (Type() != JsonType::Float) return 0;

	return _str.Trim().ToDouble();
}

Json Json::Find(cstring key) const {
	if (!_str) return Null;

	// "name": value

	// 找到名称
	int n = String(key).Length();
	int p = 0;
	while (true) {
		p = _str.IndexOf(key, p);
		if (p < 0) return Null;

		p += n;

		// 找冒号
		if (_str[p] == ':') { p++; break; }
		if (_str[p] == '"' || _str[p + 1] == ':') { p += 2; break; }
	}

	// 跳过可能的空格
	n = _str.Length() - p;
	auto val = SkipSpace(_str.GetBuffer() + p, n);
	// 找到结尾
	switch (val[0])
	{
	case '{': n = find(val, n, '}') + 1; break;
	case '[': n = find(val, n, ']') + 1; break;
	case '"': n = find(val, n, '"') + 1; break;
		// 其它类型只需要逗号，如果没有逗号，就可能是最后一个了
	default: {
		auto ve = val + n;
		for (auto vs = val; vs < ve; vs++)
		{
			if (vs[0] == ',' || vs[0] == '}' || vs[0] == ']' || isSpace(vs[0]))
			{
				n = vs - val;
				break;
			}
		}
	}
	}

	return Json(val, n);
}

// 读取成员。找到指定成员，并用它的值构造一个新的对象
const Json Json::operator[](cstring key) const { return Find(key); }

/*// 设置成员。找到指定成员，或添加成员，并返回对象
Json& Json::operator[](cstring key)
{
	//if(!_writer) return Find(key);

	Json json;

	Add(key, json);
	json.SetOut(*_writer);

	return json;
}*/

// 特殊支持数组
int Json::Length() const {
	if (!_str) return 0;

	auto cs = _str.GetBuffer();
	if (cs[0] != '[') return 0;

	int len = _str.Length();
	if (len == 2 && cs[1] == ']') return 0;

	// 跳过左方括号
	cs++;
	len--;

	// 数逗号
	int n = 0;
	while (len > 0)
	{
		int p = find(cs, len, ',');
		if (p < 0) break;

		n++;
		cs += p + 1;
		len -= p + 1;
	}

	// 数组个数就是逗号个数加一
	return n + 1;
}

const Json Json::operator[](int index) const {
	auto& json = Null;
	if (!_str) return json;

	auto cs = _str.GetBuffer();
	if (cs[0] != '[') return json;

	int len = _str.Length();
	if (len == 2 && cs[1] == ']') return json;

	// 跳过左方括号
	cs++;
	len--;

	// 数逗号
	while (len > 0)
	{
		// 找下一个逗号
		int p = find(cs, len, ',');
		if (p < 0)
		{
			// 最后没找到逗号，如果刚好index为0，说明是最后一段
			if (index == 0) {
				if (cs[len - 1] == ']') len--;
				return Json(cs, len);
			}
			break;
		}
		if (index-- == 0) return Json(cs, p);

		cs += p + 1;
		len -= p + 1;
	}

	return json;
}

/*Json& Json::operator[](int index)
{
	return *this;
}*/

Json::Json() { }

/*Json::Json(String& value) {
	_str = _str + "\"" + value + "\"";
}

Json::Json(bool value) : _str(value) { }
Json::Json(int value) : _str(value) { }
Json::Json(float value) : _str(value) { }
Json::Json(double value) : _str(value) { }*/

// 设置输出缓冲区
Json::Json(char* buf, int len) :_str(buf, len, false) { _str.SetLength(0); }

// 添加对象成员
Json& Json::Add(cstring key, const Json& value) {
	AddKey(key);

	if (value.Type() == JsonType::null)
		_str += "null";
	else
		_str += value;

	_str += key ? '}' : ']';

	return *this;
}

Json& Json::Add(cstring key, cstring value) {
	AddKey(key);

	_str = _str + "\"" + value + "\"";
	_str += key ? '}' : ']';

	return *this;
}

Json& Json::Add(cstring key, bool value) {
	AddKey(key);

	if (value)
		_str += "true";
	else
		_str += "false";

	_str += key ? '}' : ']';

	return *this;
}

Json& Json::Add(cstring key, int value) {
	AddKey(key);

	_str += value;
	_str += key ? '}' : ']';

	return *this;
}

Json& Json::Add(cstring key, float value) {
	AddKey(key);

	_str += value;
	_str += key ? '}' : ']';

	return *this;
}

Json& Json::Add(cstring key, double value) {
	AddKey(key);

	_str += value;
	_str += key ? '}' : ']';

	return *this;
}

Json& Json::Add(cstring key, const String& value) {
	AddKey(key);

	_str = _str + "\"" + value + "\"";
	_str += key ? '}' : ']';

	return *this;
}

Json& Json::Add(cstring value) { return Add(nullptr, value); }
Json& Json::Add(bool value) { return Add(nullptr, value); }
Json& Json::Add(int value) { return Add(nullptr, value); }
Json& Json::Add(float value) { return Add(nullptr, value); }
Json& Json::Add(double value) { return Add(nullptr, value); }
Json& Json::Add(const String& value) { return Add(nullptr, value); }

/*// 特殊处理字符串，避免隐式转换
Json& Json::Add(cstring key, const String& value) {
	auto& s = _str;
	int len = s.Length();
	// 如果已经有数据，则把最后的括号改为逗号
	if (len > 0)
		s[len - 1] = ',';
	else
		s += '{';

	s += '"';
	s += key;
	s += "\":\"";

	s += value;

	s += "\"}";

	return *this;
}

Json& Json::Add(cstring key, cstring value) { return Add(key, String(value)); }*/

// 添加数组成员
/*Json& Json::Add(const Json& value) {
	auto& s = _str;
	int len = s.Length();
	// 如果已经有数据，则把最后的括号改为逗号
	if (len > 0)
		s[len - 1] = ',';
	else
		s += '[';

	s += value;

	s += ']';

	return *this;
}*/

/*Json Json::AddObject(cstring key) {
	return Null;
}

Json Json::AddArray(cstring key) {
	return Null;
}*/

// 添加键
void Json::AddKey(cstring key) {
	// 数组没有key
	auto& s = _str;
	int len = s.Length();
	// 如果已经有数据，则把最后的括号改为逗号
	if (len > 0)
		s[len - 1] = ',';
	else if (key)
		s += '{';
	else
		s += '[';

	// 数组没有键和冒号
	if (key) {
		s += '"';
		s += key;
		s += "\":";
	}
}

String Json::ToString() const { return _str; }

void Json::Show(bool newline) const {
	_str.Show(newline);
}

static bool isSpace(char ch) {
	return ch == ' ' ||
		ch == '\t' ||
		ch == '\r' ||
		ch == '\n';
}

// 跳过空白
static cstring SkipSpace(cstring str, int& len) {
	while (len && isSpace(str[0])) { str++; len--; }
	while (len && isSpace(str[len - 1])) len--;

	return str;
}

static int find(cstring str, int len, char ch) {
	// 记录大括号、中括号配对
	int m = 0;
	int n = 0;
	// 是否在字符串中，此时不算括号
	bool s = false;
	for (int i = 0; i < len; i++)
	{
		switch (str[i])
		{
		case '{': if (!s) m++; break;
		case '}': if (!s) m--; break;
		case '[': if (!s) n++; break;
		case ']': if (!s) n--; break;
		case '"':
			if (i == 0 || str[i - 1] != '\\') s = !s;
			break;
		}

		// 配对平衡的时候遇到目标，才是结果
		if (str[i] == ch && m == 0 && n == 0 && !s) return i;
	}
	return -1;
}

/*
JValue::JValue() : type_t(NIL) { }

JValue::JValue(Int64 i) : int_v(i), type_t(INT) { }

JValue::JValue(int i) : int_v(i), type_t(INT) { }

JValue::JValue(double f) : float_v(f), type_t(FLOAT) { }

JValue::JValue(bool b) : bool_v(b), type_t(BOOL) { }

JValue::JValue(cstring s) : string_v(s), type_t(STRING) { }

JValue::JValue(const String& s) : string_v(s.GetBuffer()), type_t(STRING) { }

JValue::JValue(const JObject& o) : object_v(o), type_t(OBJECT) { }

JValue::JValue(const JArray& o) : array_v(o), type_t(ARRAY) { }

JValue::JValue(String&& s) : string_v(s), type_t(STRING) { }

JValue::JValue(JObject&& o) : object_v(o), type_t(OBJECT) { }

JValue::JValue(JArray&& o) : array_v(o), type_t(ARRAY) { }

JValue::JValue(const JValue& v)
{
	switch(v.type())
	{
		case INT:
			int_v = v.int_v;
			type_t = INT;
			break;

		case FLOAT:
			float_v = v.float_v;
			type_t = FLOAT;
			break;

		case BOOL:
			bool_v = v.bool_v;
			type_t = BOOL;
			break;

		case NIL:
			type_t = NIL;
			break;

		case STRING:
			string_v = v.string_v;
			type_t = STRING;
			break;

			case ARRAY:
			array_v = v.array_v;
			type_t = ARRAY;
			break;

		case OBJECT:
			object_v = v.object_v;
			type_t = OBJECT;
			break;
	}
}

JValue::JValue(JValue&& v)
{
	switch(v.type())
	{
		case INT:
			int_v	= (v.int_v);
			type_t	= INT;
			break;

		case FLOAT:
			float_v	= (v.float_v);
			type_t	= FLOAT;
			break;

		case BOOL:
			bool_v	= (v.bool_v);
			type_t	= BOOL;
			break;

		case NIL:
			type_t	= NIL;
			break;

		case STRING:
			string_v= (v.string_v);
			type_t	= STRING;
			break;

			case ARRAY:
			array_v	= (v.array_v);
			type_t	= ARRAY;
			break;

		case OBJECT:
			object_v= (v.object_v);
			type_t	= OBJECT;
			break;
	}
}

JValue& JValue::operator=(const JValue& v)
{
	switch(v.type())
	{
		case INT:
			int_v = v.int_v;
			type_t = INT;
			break;

		case FLOAT:
			float_v = v.float_v;
			type_t = FLOAT;
			break;

		case BOOL:
			bool_v = v.bool_v;
			type_t = BOOL;
			break;

		case NIL:
			type_t = NIL;
			break;

		case STRING:
			string_v = v.string_v;
			type_t = STRING;
			break;

			case ARRAY:
			array_v = v.array_v;
			type_t = ARRAY;
			break;

		case OBJECT:
			object_v = v.object_v;
			type_t = OBJECT;
			break;
	}

	return *this;

}

JValue& JValue::operator=(JValue&& v)
{
	switch(v.type())
	{
		case INT:
			int_v = (v.int_v);
			type_t = INT;
			break;

		case FLOAT:
			float_v = (v.float_v);
			type_t = FLOAT;
			break;

		case BOOL:
			bool_v = (v.bool_v);
			type_t = BOOL;
			break;

		case NIL:
			type_t = NIL;
			break;

		case STRING:
			string_v = (v.string_v);
			type_t = STRING;
			break;

			case ARRAY:
			array_v = (v.array_v);
			type_t = ARRAY;
			break;

		case OBJECT:
			object_v = (v.object_v);
			type_t = OBJECT;
			break;
	}

	return *this;

}

JValue& JValue::operator[] (cstring key)
{
	//if (type() != OBJECT)
	//    throw ("JValue not an object");
	return object_v[key];
}

const JValue& JValue::operator[] (cstring key) const
{
	//if (type() != OBJECT)
	//    throw ("JValue not an object");
	return object_v[key];
}

JValue& JValue::operator[] (uint i)
{
	//if (type() != ARRAY)
	//    throw ("JValue not an array");
	return array_v[i];
}

const JValue& JValue::operator[] (uint i) const
{
	//if (type() != ARRAY)
	//    throw ("JValue not an array");
	return array_v[i];
}


JObject::JObject() { }

JObject::~JObject() { }

JObject::JObject(const JObject& o) : _items(o._items) { }

JObject::JObject(JObject&& o) : _items((o._items)) { }

JObject& JObject::operator=(const JObject& o)
{
	_items = o._items;
	return *this;
}

JObject& JObject::operator=(JObject&& o)
{
	_items = (o._items);
	return *this;
}

JValue& JObject::operator[] (cstring key)
{
	return *_items[key];
}

const JValue& JObject::operator[] (cstring key) const
{
	return *_items[key];
}

void JObject::Add(cstring key, JValue& value)
{
	_items.Add(key, &value);
}

int JObject::size() const
{
	return _items.Count();
}

String JObject::ToString() const
{
	String str;
	auto& keys	= _items.Keys();
	auto& vals	= _items.Values();
	str	+= '{';
	for(int i=0; i<size(); i++)
	{
		if(i > 0) str	+= ',';

		auto key	= (cstring)keys[i];
		str	= str + '"' + key + '"' + ':';

		auto& val	= *(JValue*)vals[i];
		str	+= val;
	}
	str	+= '}';

	return str;
}

JArray::JArray() { }

JArray::~JArray() { }

JArray::JArray(const JArray& a) : _array(a._array) { }

JArray::JArray(JArray&& a) : _array((a._array)) { }

JArray& JArray::operator=(const JArray& a)
{
	_array = a._array;
	return *this;
}

JArray& JArray::operator=(JArray&& a)
{
	_array = (a._array);
	return *this;
}


JValue& JArray::operator[] (uint i)
{
	return *_array[i];
}

const JValue& JArray::operator[] (uint i) const
{
	return *_array[i];
}

int JArray::size() const
{
	return _array.Count();
}

void JArray::Add(const JValue& v)
{
	_array.Add(&(JValue&)v);
}

String JArray::ToString() const
{
	String str;
	str	+= '[';
	for(int i=0; i<size(); i++)
	{
		if(i > 0) str	+= ',';

		auto& v	= (*this)[i];
		str	+= v;
	}
	str	+= ']';

	return str;
}

String JValue::ToString() const
{
	String str;
	auto& v	= *this;
	switch(v.type())
	{
		case INT:
			str	+= (int)v;
			break;

		case FLOAT:
			str	+= (double)v;
			break;

		case BOOL:
			str	+= ((bool)v ? "true" : "false");
			break;

		case NIL:
			str	+= "null";
			break;

		case STRING:
			str	= str + '"' + (String)v + '"';
			break;

		case ARRAY:
			str	+= (JArray&)v;
			break;

		case OBJECT:
			str	+= (JObject&)v;
			break;
	}

	return str;
}
*/

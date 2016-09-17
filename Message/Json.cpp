#include "Json.h"

static bool isSpace(char ch);
static cstring SkipSpace(cstring str, int& len);
static int find(cstring str, int len, char ch);

static const Json Null;

Json::Json() { Init(nullptr, 0); }

Json::Json(cstring str)
{
	Init(str, String(str).Length());
}

void Json::Init(cstring str, int len)
{
	//_str	= str;
	_len	= len;

	_str	= SkipSpace(str, _len);
}

Json::Json(int value)
{
	
}

Json::Json(bool value)
{
	
}

Json::Json(double value)
{
	
}

Json::Json(String& value)
{
	
}

// 值类型
JsonType Json::Type() const
{
	if(!_str && !_len) return JsonType::null;

	// 快速判断对象、数组和字符串
	switch(_str[0])
	{
		case '{': return JsonType::object;
		case '[': return JsonType::array;
		case '"': return JsonType::string;
	}

	// 对比判断空和布尔类型
	String s((cstring)_str, _len);
	if(s == "null") return JsonType::null;
	if(s == "true" || s == "false") return JsonType::boolean;

	// 剩下整数和浮点数
	bool isFloat	= false;
	for(int i=0; i<_len; i++)
	{
		char ch	= _str[i];
		// 判断非数字
		if(ch < '0' || ch > '9')
		{
			// 负号只能出现在第一位
			if(ch == '-' && i > 0) return JsonType::null;

			// 直接判断圆点，不支持指数类型
			if(ch == '.' && !isFloat)
				isFloat	= true;
			else
				return JsonType::null;
		}
	}

	return isFloat ? JsonType::Float : JsonType::integer;
}

// 获取值
String	Json::AsString()	const
{
	if(!_str && !_len) return nullptr;

	if(_str[0] != '"') return nullptr;

	// 去掉前后双引号
	auto p	= _str;
	int n	= _len;
	if(p[0] == '"')
	{
		p++;
		n--;
	}
	if(p[n-1] == '"') n--;

	// 没有处理转义字符
	return String(p, n);
}

bool	Json::AsBoolean()	const
{
	if(!_str && !_len) return false;

	if(_str[0] != 't' && _str[0] != 'f') return false;

	String s((cstring)_str, _len);
	return s == "true";
}

int		Json::AsInt()		const
{
	if(!_str && !_len) return 0;

	if(Type() != JsonType::integer) return 0;

	String s((cstring)_str, _len);
	return s.ToInt();
}

float	Json::AsFloat()	const
{
	if(!_str && !_len) return 0;

	if(Type() != JsonType::Float) return 0;

	String s((cstring)_str, _len);
	return s.ToFloat();
}

double	Json::AsDouble()	const
{
	if(!_str && !_len) return 0;

	if(Type() != JsonType::Float) return 0;

	String s((cstring)_str, _len);
	return s.ToDouble();
}

Json Json::Find(cstring key) const
{
	Json json;
	if(!_str && !_len) return json;

	String s((cstring)_str, _len);

	// "name": value

	// 找到名称
	int n	= String(key).Length();
	int p	= 0;
	while(true){
		p	= s.IndexOf(key, p);
		if(p < 0) return json;

		p	+= n;

		// 找冒号
		if(_str[p] == ':') { p++; break; }
		if(_str[p] == '"' || _str[p+1] == ':') { p+=2; break; }
	}

	// 跳过可能的空格
	n	= _len - p;
	auto val	= SkipSpace(_str + p, n);
	// 找到结尾
	switch(val[0])
	{
		case '{': n	= find(val, n, '}'); break;
		case '[': n	= find(val, n, ']'); break;
		case '"': n	= find(val, n, '"'); break;
		// 其它类型只需要逗号，如果没有逗号，就可能是最后一个了
		default: {
			auto ve	= val + n;
			for(auto vs=val; vs < ve; vs++)
			{
				if(vs[0] == ',' || isSpace(vs[0]))
				{
					n	= vs - val;
					break;
				}
			}
		}
	}

	json.Init(val, n);

	return json;
}

// 读取成员。找到指定成员，并用它的值构造一个新的对象
const Json Json::operator[](cstring key) const { return Find(key); }

// 设置成员。找到指定成员，或添加成员，并返回对象
Json& Json::operator[](cstring key)
{
	//if(!_s) return Find(key);

	Json json;

	Add(key, json);
	json.SetOut(*_s);

	return json;
}

// 特殊支持数组
int Json::Length() const
{
	if(!_str && !_len) return 0;

	if(_str[0] != '[') return 0;
	if(_len == 2 && _str[1] == ']') return 0;

	// 数逗号
	int n	= 0;
	for(int i=0; i<_len;)
	{
		int p	= find(_str + i + 1, _len - i - 1, ',');
		if(p < 0) break;
		i	+= p;
	}

	// 数组个数就是逗号个数加一
	return n + 1;
}

const Json Json::operator[](int index) const
{
	Json json;
	if(!_str && !_len) return json;

	if(_str[0] != '[') return json;
	if(_len == 2 && _str[1] == ']') return json;

	// 数逗号
	for(int i=0; i<_len;)
	{
		// 找下一个逗号
		auto s	= _str + i + 1;
		int p	= find(s, _len - i - 1, ',');
		if(p < 0)
		{
			// 最后没找到逗号，如果刚好index为0，说明是最后一段
			if(index == 0) json.Init(s, _len - i - 1);
			break;
		}
		i	+= p;

		if(index-- == 0)
		{
			json.Init(s, p);
			break;
		}
	}

	return json;
}

/*Json& Json::operator[](int index)
{
	return *this;
}*/

// 设置输出缓冲区。写入Json前必须设置
void Json::SetOut(String& result)
{
	_s	= &result;
}

void Json::Check()
{
	if(!_s) _s	= new String();
}

// 添加对象成员
Json& Json::Add(cstring key, const Json& value)
{
	auto& s	= *_s;
	// 如果已经有数据，则把最后的括号改为逗号
	if(s.Length() > 0)
		s[s.Length() - 1]	= ',';
	else
		s	+= '{';

	s	+= '"';
	s	+= key;
	s	+= "\":";

	//s	+= value;
	value.ToStr(s);

	s	+= '}';

	return *this;
}

// 添加数组成员
Json& Json::Add(const Json& value)
{
	auto& s	= *_s;
	// 如果已经有数据，则把最后的括号改为逗号
	if(s.Length() > 0)
		s[s.Length() - 1]	= ',';
	else
		s	+= '[';

	value.ToStr(s);

	s	+= ']';

	return *this;
}

String& Json::ToStr(String& str) const
{
	if(_s) str	+= *_s;

	return str;
}

static bool isSpace(char ch)
{
	return ch == ' ' ||
		ch == '\t' ||
		ch == '\r' ||
		ch == '\n';

}

// 跳过空格
static cstring SkipSpace(cstring str, int& len)
{
	while(len && isSpace(str[0])) { str++; len--; }
	while(len && isSpace(str[len -1])) len--;

	return str;
}

static int find(cstring str, int len, char ch)
{
	// 记录大括号、中括号配对
	int m	= 0;
	int n	= 0;
	// 是否在字符串中，此时不算括号
	bool s	= false;
	for(int i=0; i<len; i++)
	{
		switch(str[i])
		{
			case '{': if(!s) m++; break;
			case '}': if(!s) m--; break;
			case '[': if(!s) n++; break;
			case ']': if(!s) n--; break;
			case '"':
				if(i == 0 || str[i-1] != '\\') s	= !s;
				break;
		}

		// 配对平衡的时候遇到目标，才是结果
		if(str[i] == ch && m == 0 && n == 0 && !s) return i;
	}
	return -1;
}

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
    if (type() != OBJECT)
        throw ("JValue not an object");
    return object_v[key];
}

const JValue& JValue::operator[] (cstring key) const
{
    if (type() != OBJECT)
        throw ("JValue not an object");
    return object_v[key];
}

JValue& JValue::operator[] (uint i)
{
    if (type() != ARRAY)
        throw ("JValue not an array");
    return array_v[i];
}

const JValue& JValue::operator[] (uint i) const
{
    if (type() != ARRAY)
        throw ("JValue not an array");
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

uint JObject::size() const
{
    return _items.Count();
}

String& JObject::ToStr(String& str) const
{
	auto& keys	= _items.Keys();
	auto& vals	= _items.Values();
	str	+= '{';
	for(int i=0; i<size(); i++)
	{
		if(i > 0) str	+= ',';

		auto key	= (cstring)keys[i];
		str	= str + '"' + key + '"' + ':';

		auto& val	= *(JValue*)vals[i];
		val.ToStr(str);
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

uint JArray::size() const
{
    return _array.Count();
}

void JArray::Add(const JValue& v)
{
    _array.Add(&(JValue&)v);
}

String& JArray::ToStr(String& str) const
{
	str	+= '[';
	for(int i=0; i<size(); i++)
	{
		if(i > 0) str	+= ',';

		auto& v	= (*this)[i];
		v.ToStr(str);
	}
	str	+= ']';

	return str;
}

String& JValue::ToStr(String& str) const
{
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
            ((JArray)v).ToStr(str);
            break;

        case OBJECT:
            ((JObject)v).ToStr(str);
            break;
    }

	return str;
}

#include "Json.h"

static const Json Null;

Json::Json() { Init(nullptr, 0); }

Json::Json(cstring str)
{
	Init(str, String(str).Length());
}

void Json::Init(cstring str, int len)
{
	_str	= str;
	_len	= len;
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
cstring	Json::AsString()	const
{
	if(!_str && !_len) return nullptr;

	if(_str[0] != '"') return nullptr;

	return _str;
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

// 跳过空格
cstring SkipSpace(cstring str)
{
	while(str[0] == ' ' ||
		str[0] == '\t' ||
		str[0] == '\r' ||
		str[0] == '\n') str++;

	return str;
}

// 读取对象结尾
int readObject(cstring str, int len)
{
	// { ... }
	if(str[0] != '{') return 0;

	// 记录括号配对
	int m	= 0;
	// 是否在字符串中，此时不算括号
	bool s	= false;
	for(int i=0; i<len; i++)
	{
		switch(str[i])
		{
			case '{': if(!s) m++; break;
			case '}': {
				if(!s) m--;
				if(!m) return i + 1;
				break;
			}
			case '"': {
				if(str[i-1] != '\\') s	= !s;
			}
		}
	}
	return 0;
}

// 读取数组结尾
int readArray(cstring str, int len)
{
	// [ ... ]
	if(str[0] != '[') return 0;

	// 记录括号配对
	int m	= 0;
	// 是否在字符串中，此时不算括号
	bool s	= false;
	for(int i=0; i<len; i++)
	{
		switch(str[i])
		{
			case '[': if(!s) m++; break;
			case ']': {
				if(!s) m--;
				if(!m) return i + 1;
				break;
			}
			case '"': {
				if(str[i-1] != '\\') s	= !s;
			}
		}
	}
	return 0;
}

// 读取字符串结尾
int readString(cstring str, int len)
{
	// " ..\".. "
	if(str[0] != '"') return 0;

	// 找到第一个不是转义的双引号
	for(int i=1; i<len; i++)
	{
		switch(str[i])
		{
			case '"': {
				if(str[i-1] != '\\') return i + 1;
				break;
			}
		}
	}

	return 0;
}

// 读取成员。找到指定成员，并用它的值构造一个新的对象
Json Json::operator[](cstring key) const
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
	auto val	= SkipSpace(_str + p);
	n	= _str + _len - val;
	// 找到结尾
	switch(val[0])
	{
		case '{': n	= readObject(val, n); break;
		case '[': n	= readArray(val, n); break;
		case '"': n	= readString(val, n); break;
		// 其它类型只需要逗号，如果没有逗号，就可能是最后一个了
		default: {
			auto ve	= val + n;
			for(auto vs=val; vs < ve; vs++)
			{
				if(vs[0] == ',')
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

// 设置成员。找到指定成员，或添加成员，并返回对象
/*Json& Json::operator[](cstring key)
{
	return *this;
}*/

// 特殊支持数组
int Json::Length() const
{
	if(!_str && !_len) return 0;

	if(_str[0] != '[') return 0;

	return 0;
}

Json Json::operator[](int index) const
{
	return *this;
}

Json& Json::operator[](int index)
{
	return *this;
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

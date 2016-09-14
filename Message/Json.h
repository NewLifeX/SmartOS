#ifndef __Json_H__
#define __Json_H__

#include "Type.h"
#include "SString.h"
#include "List.h"
#include "Dictionary.h"

/*
一个Json对象内部包含有一个字符串，读取成员就是截取子字符串构建新的Json对象。
要么只读，要么只写，不允许读取后修改，不允许写入后读取。
*/

enum class JsonType : byte
{
	null,
	object,
	array,
	string,
	boolean,
	integer,
	Float
};

// Json对象
class Json : public Object
{
public:
	static Json Null;

	Json();
	Json(cstring str);

	// 值类型
	JsonType Type() const;

	// 获取值
	String	AsString()	const;
	bool	AsBoolean()	const;
	int		AsInt()		const;
	float	AsFloat()	const;

	// 读取成员。找到指定成员，并用它的值构造一个新的对象
	Json operator[](cstring key) const;
	// 设置成员。找到指定成员，或添加成员，并返回对象
	//Json& operator[](cstring key);

	// 特殊支持数组
	int Length() const;
	Json operator[](int index) const;
	Json& operator[](int index);

#if DEBUG
	static void Test();
#endif
	
private:
	cstring	_str;
	int		_len;

	void Init(cstring str, int len);
};

/** Json值类型 */
enum ValueType
{
	INT,
	FLOAT,
	BOOL,
	STRING,
	OBJECT,
	ARRAY,
	NIL
};

class JValue;

// Json对象
class JObject
{
public:

	JObject();
	JObject(const JObject& o);
	JObject(JObject&& o);

	JObject& operator=(const JObject& o);
	JObject& operator=(JObject&& o);

	~JObject();

	JValue& operator[] (cstring key);
	const JValue& operator[] (cstring key) const;

	// 插入
	void Add(cstring key, JValue& value);

	// 大小
	uint size() const;

	String& ToStr(String& str) const;

protected:

	// 内部容器
	Dictionary<cstring, JValue*> _items;
};

// Json数组
class JArray
{
public:

	JArray();

	~JArray();

	JArray(const JArray& a);
	JArray(JArray&& a);

	JArray& operator=(const JArray& a);
	JArray& operator=(JArray&& a);

	JValue& operator[] (uint i);
	const JValue& operator[] (uint i) const;

	// 添加
	void Add(const JValue& n);

	// 大小
	uint size() const;

	String& ToStr(String& str) const;

protected:

	// 内部容器
	List<JValue*> _array;
};

// Json值
class JValue
{
public:

	JValue();
	JValue(const JValue& v);
	JValue(Int64 i);
	JValue(int i);
	JValue(double f);
	JValue(bool b);
	JValue(cstring s);
	JValue(const String& s);
	JValue(const JObject& o);
	JValue(const JArray& a);

	JValue(JValue&& v);
	JValue(String&& s);
	JValue(JObject&& o);
	JValue(JArray&& a);

	ValueType type() const
	{
		return type_t;
	}

	JValue& operator[] (cstring key);
	const JValue& operator[] (cstring key) const;

	JValue& operator[] (uint i);
	const JValue& operator[] (uint i) const;

	JValue& operator=(const JValue& v);
	JValue& operator=(JValue&& v);

	explicit operator double()	const { return float_v; }
	explicit operator int()		const { return int_v; }
	explicit operator bool()	const { return bool_v; }
	explicit operator String()	const { return string_v; }

	operator JObject ()	const { return object_v; }
	operator JArray ()	const { return array_v; }

	double as_float()	const { return float_v; }
	int as_int()		const { return int_v; }
	bool as_bool()		const { return bool_v; }
	String as_string()	const { return string_v; }

	String& ToStr(String& str) const;


protected:

	double	float_v;
	Int64	int_v;
	bool	bool_v;
	String	string_v;

	JObject	object_v;
	JArray	array_v;

	ValueType	type_t;
};

#endif

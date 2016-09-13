#ifndef __Json_H__
#define __Json_H__

#include "Type.h"
#include "SString.h"
#include "List.h"
#include "Dictionary.h"

/** Json值类型 */
enum ValueType
{
	INT,        // JSON's int
	FLOAT,      // JSON's float 3.14 12e-10
	BOOL,       // JSON's boolean (true, false)
	STRING,     // JSON's string " ... " or (not really JSON) ' ... '
	OBJECT,     // JSON's object { ... }
	ARRAY,      // JSON's array [ ... ]
	NIL         // JSON's null
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

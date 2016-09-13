#include "Json.h"


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

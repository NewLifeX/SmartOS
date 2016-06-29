#include "Type.h"
#include "Buffer.h"
#include "SString.h"

#include "List.h"
#include "Dictionary.h"

IDictionary::IDictionary(IComparer comparer)
{
	_Keys.Comparer	= comparer;
}

int IDictionary::Count() const { return _Keys.Count(); }
const IList& IDictionary::Keys()	const { return _Keys; }
const IList& IDictionary::Values()	const { return _Values; }

// 添加单个元素
void IDictionary::Add(PKey key, void* value)
{
	// 判断一下，如果已存在，则覆盖
	int idx	= _Keys.FindIndex(key);
	if(idx >= 0)
	{
		_Values[idx]	= value;
	}
	else
	{
		_Keys.Add((void*)key);
		_Values.Add(value);
	}
}

// 删除指定元素
void IDictionary::Remove(PKey key)
{
	int idx = _Keys.Remove(key);
	if(idx >= 0) _Values.RemoveAt(idx);
}

void IDictionary::Clear()
{
	_Keys.Clear();
	_Values.Clear();
}

// 是否包含指定项
bool IDictionary::ContainKey(PKey key) const
{
	return _Keys.FindIndex(key) >= 0;
}

// 尝试获取值
bool IDictionary::TryGetValue(PKey key, void*& value) const
{
	int idx	= _Keys.FindIndex(key);
	if(idx < 0) return false;

	value	= _Values[idx];

	return true;
}


// 重载索引运算符[]，返回指定元素的第一个
void* IDictionary::operator[](PKey key) const
{
	int idx	= _Keys.FindIndex(key);
	if(idx < 0) return nullptr;

	return _Values[idx];
}

void*& IDictionary::operator[](PKey key)
{
	int idx	= _Keys.FindIndex(key);
	if(idx < 0)
	{
		static void* dummy;
		return dummy;
	}

	return _Values[idx];
}

const String IDictionary::GetString(PKey key) const
{
	void* p	= nullptr;
	TryGetValue(key, p);
	
	return String((cstring)p);
}

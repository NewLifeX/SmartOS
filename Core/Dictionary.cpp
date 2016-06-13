#include "Type.h"
#include "Buffer.h"
#include "List.h"
#include "Dictionary.h"

Dictionary::Dictionary(IComparer comparer)
{
	_Keys.Comparer	= comparer;
}

int Dictionary::Count() const { return _Keys.Count(); }
const List& Dictionary::Keys() const { return _Keys; }
const List& Dictionary::Values() const { return _Values; }

// 添加单个元素
void Dictionary::Add(const void* key, void* value)
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
void Dictionary::RemoveKey(const void* key)
{
	int idx = _Keys.Remove(key);
	if(idx >= 0) _Values.RemoveAt(idx);
}

void Dictionary::Clear()
{
	_Keys.Clear();
	_Values.Clear();
}

// 是否包含指定项
bool Dictionary::ContainKey(const void* key) const
{
	return _Keys.FindIndex(key) >= 0;
}

// 尝试获取值
bool Dictionary::TryGetValue(const void* key, void*& value) const
{
	int idx	= _Keys.FindIndex(key);
	if(idx < 0) return false;

	value	= _Values[idx];

	return true;
}


// 重载索引运算符[]，返回指定元素的第一个
void* Dictionary::operator[](const void* key) const
{
	int idx	= _Keys.FindIndex(key);
	if(idx < 0) return nullptr;

	return _Values[idx];
}

void*& Dictionary::operator[](const void* key)
{
	int idx	= _Keys.FindIndex(key);
	if(idx < 0)
	{
		static void* dummy;
		return dummy;
	}

	return _Values[idx];
}

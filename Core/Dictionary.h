#ifndef _Dictionary_H_
#define _Dictionary_H_

// 字典。仅用于存储指针。
// 内置两个List用于存储键值集合，添加删除时对它们进行同步操作
class IDictionary
{
	typedef const void*	PKey;
	typedef void*		PValue;
public:
    IDictionary(IComparer comparer = nullptr);

	int Count() const;
	const IList& Keys() const;
	const IList& Values() const;

	// 添加单个元素
    void Add(PKey key, PValue value);

	// 删除指定元素
	void Remove(PKey key);

	void Clear();

	// 是否包含指定项
	bool ContainKey(PKey key) const;

	// 尝试获取值
	bool TryGetValue(PKey key, PValue& value) const;

    // 重载索引运算符[]，返回指定元素的第一个
    PValue operator[](PKey key) const;
    PValue& operator[](PKey key);

	const String GetString(PKey key) const;

#if DEBUG
	static void Test();
#endif

private:
	IList	_Keys;
	IList	_Values;
};

template<typename TKey, typename TValue>
class Dictionary : public IDictionary
{
	typedef const TKey	PKey;
	typedef TValue		PValue;
public:
	const List<TKey>& Keys() const;
	const List<TValue>& Values() const;

	// 添加单个元素
    void Add(PKey key, PValue value) { IDictionary::Add(key, value); }

	// 删除指定元素
	void Remove(PKey key) { IDictionary::Remove(key); }

	// 是否包含指定项
	bool ContainKey(PKey key) const { return IDictionary::ContainKey(key); }

	// 尝试获取值
	bool TryGetValue(PKey key, PValue& value) const { return IDictionary::TryGetValue(key, value); }

    // 重载索引运算符[]，返回指定元素的第一个
    PValue operator[](PKey key) const	{ return IDictionary::operator[](key); }
    PValue& operator[](PKey key)		{ return IDictionary::operator[](key); }
};

#endif

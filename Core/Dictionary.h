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

	inline int Count()				const { return _Keys.Count(); }
	inline const IList& Keys()		const { return _Keys; }
	inline const IList& Values()	const { return _Values; }

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
	static_assert(sizeof(TKey) <= 4, "Dictionary only support pointer or int");
	static_assert(sizeof(TValue) <= 4, "Dictionary only support pointer or int");

	typedef const TKey	PKey;
	typedef TValue		PValue;
public:
    Dictionary(IComparer comparer = nullptr) : IDictionary(comparer) { }

	const List<TKey>& Keys() const		{ return (List<TKey>&)	 IDictionary::Keys();	};
	const List<TValue>& Values() const	{ return (List<TValue>&) IDictionary::Values();	};

	// 添加单个元素
    void Add(PKey key, PValue value) { IDictionary::Add((const void*)key, (void*)value); }

	// 删除指定元素
	void Remove(PKey key) { IDictionary::Remove((const void*)key); }

	// 是否包含指定项
	bool ContainKey(PKey key) const { return IDictionary::ContainKey((const void*)key); }

	// 尝试获取值
	bool TryGetValue(PKey key, PValue& value) const
	{
		void* val	= nullptr;
		bool rs	= IDictionary::TryGetValue((const void*)key, val);
		value	= (PValue)val;

		return rs;
	}

    // 重载索引运算符[]，返回指定元素的第一个
    PValue operator[](PKey key) const	{ return IDictionary::operator[]((const void*)key); }
    PValue& operator[](PKey key)		{ return IDictionary::operator[]((const void*)key); }
};

#endif

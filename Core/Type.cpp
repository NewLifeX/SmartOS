#include "Type.h"

#include "Buffer.h"
#include "Array.h"
#include "ByteArray.h"
#include "SString.h"

#include <typeinfo>
using namespace ::std;

/******************************** Object ********************************/

// 输出对象的字符串表示方式
String& Object::ToStr(String& str) const
{
	auto name = typeid(*this).name();
	while(*name >= '0' && *name <= '9') name++;

	str	+= name;

	return str;
}

// 输出对象的字符串表示方式。支持RVO优化。
// 该方法直接返回给另一个String作为初始值，只有一次构造，没有多余构造、拷贝和析构。
String Object::ToString() const
{
	String str;
	ToStr(str);

	return str;
}

void Object::Show(bool newLine) const
{
	// 为了减少堆分配，采用较大的栈缓冲区
	char cs[0x200];
	String str(cs, ArrayLength(cs));
	ToStr(str);
	str.Show(newLine);
}

const Type Object::GetType() const
{
	auto p = (int*)this;

	//return Type(&typeid(*this), *(p - 1));
	
	Type type;
	
	type._info	= &typeid(*this);
	type.Size	= *(p - 1);
	
	return type;
}

/******************************** Type ********************************/

Type::Type()
{
	//_info	= ti;

	//Size	= size;
}

const String Type::Name() const
{
	auto ti	= (const type_info*)_info;
	auto name = ti->name();
	while(*name >= '0' && *name <= '9') name++;

	return String(name);
}

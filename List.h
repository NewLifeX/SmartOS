#ifndef _List_H_
#define _List_H_

#include "Sys.h"

// 列表模版
template<typename T>
class List
{
public:
    List() { _count = _total = 0; }
    List(int size = 4)
    {
        _count = 0;
        _total = size;
        arr = new T[size];
    }
    List(T* items) { }

    ~List() { if(arr) delete[] arr; arr = NULL; }

    void Add(T item)
    {
        // 检查大小
        CheckSize();
        
        arr[_count++] = item;
    }
    
    void Add(T* items, int count)
    {
        int size = _count + count;
        if(size >= _total) ChangeSize(size * 2);

        for(int i=0; i<count; i++)
        {
            arr[_count++] = *items++;
        }
    }

    T* ToArray()
    {
        // 如果刚好完整则直接返回，否则重新调整空间
        if(_count != _total)
        {
            T* arr2 = new T[_count];
            memcpy(arr, arr2, _count);
            delete[] arr;
            arr = arr2;
        }
        return arr;
    }
    int Count() { return _count; }

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。内部不检查下标越界，外部好自为之
    T operator[](int i) { return arr[i]; }
    T* operator=(List list) { return list.ToArray(); }
private:
    T* arr;
    uint _count;
    uint _total;

    void ChangeSize(int newSize)
    {
        if(_total == newSize) return;
        
        T* arr2 = new T[newSize];
        if(arr)
        {
            // 如果新数组较小，则直接复制；如果新数组较大，则先复制，再清空余下部分
            if(newSize < _total)
                memcpy(arr, arr2, newSize);
            else
            {
                memcpy(arr, arr2, _total);
                memset(arr2 + _total, newSize - _total);
            }
            delete[] arr;
        }
        arr = arr2;
    }
    
    void CheckSize()
    {
        // 如果数组空间已用完，则两倍扩容
        if(_count >= _total) ChangeSize(_count * 2);
    }
};

#endif //_List_H_

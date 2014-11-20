#include "Memory.h"

int __fastcall malloc(int a1)
{
  unsigned int size; // r1@1
  LinkNode **i; // r2@2
  int v3; // r3@3
  int v4; // r4@5
  LinkNode *v5; // r3@5
  LinkNode *v6; // r5@5
  LinkNode *v7; // r3@5
  int result; // r0@7
  LinkNode *node; // r0@9
  LinkNode *v10; // r0@15

  size = 8 * ((unsigned int)(a1 + 11) >> 3);
  while ( 1 )
  {
    for ( i = (LinkNode **)&_microlib_freelist; ; i = &node->next )
    {                                           // 遍历链表，如果节点为空，则转去初始化或者退出
      node = *i;
      if ( !*i )
      {
        result = 0;
        goto LABEL_11;
      }
      v3 = node->size;                          // 如果该节点剩余大小足够，就用它了
      if ( node->size >= size )
        break;
    }
    if ( v3 <= size )                           // 节点大小小于size的情况不存在，上面遍历必须找到一个合适大小，否则就返回空
    {
      v7 = node->next;                          // 等于还有可能。如果等于，当前节点存储的下一个节点指向再下一个节点。等于抽走了当前节点
    }
    else
    {
      v4 = v3 - size;                           // 节点大小略大，就拆分为两个节点，用前一个，后一个作为空余
      v5 = &node[size / 8];                     // (LinkNode*)((void*)node + size)
      v6 = node->next;
      v5->size = v4;
      v5->next = v6;
      v7 = &node[size / 8];                     // (LinkNode*)((void*)node + size)
    }
    *i = v7;
    node->size = size;
    result = (int)&node->next;
LABEL_11:
    if ( result )
      return result;
    if ( _microlib_freelist_initialised )
      return 0;
    v10 = (LinkNode *)((char *)&_heap_base + 4);
    _microlib_freelist = (int)((char *)&_heap_base + 4);
    v10->size = 8 * ((unsigned int)(&_heap_limit - (_UNKNOWN *)((char *)&_heap_base + 4)) >> 3);
    v10->next = 0;
    _microlib_freelist_initialised = 1;
  }
}

LinkNode *__fastcall free(LinkNode *result)
{
  LinkNode *prev; // r2@2
  LinkNode *node; // r1@2

  if ( result )
  {
    prev = 0;
    result = CONTAINING_RECORD(result, LinkNode, next);
    for ( node = (LinkNode *)_microlib_freelist; node && node <= result; node = (LinkNode *)node->next )
      prev = node;                              // 找到要释放节点的前一个节点和下一个节点
    if ( prev )
    {                                           // 如果要释放的节点刚好紧挨着前一个节点
      if ( (char *)result - (char *)prev == prev->size )
      {
        prev->size += result->size;             // 合并两个节点，使用前一个节点
        result = prev;
      }
      else
      {
        prev->next = result;                    // 否则，让前一个节点指向被释放节点
      }
    }
    else
    {
      _microlib_freelist = (int)result;         // 如果找不到节点，说明链表为空，使用被释放节点作为第一个节点
    }
    if ( node )                                 // 如果被释放节点后面紧跟着后一个节点，合并两个节点
    {
      if ( (char *)node - (char *)result == result->size )
      {
        result->size += node->size;
        node = (LinkNode *)node->next;
      }
    }
    result->next = node;
  }
  return result;
}

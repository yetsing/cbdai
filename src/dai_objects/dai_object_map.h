#ifndef CBDAI_DAI_OBJECT_MAP_H
#define CBDAI_DAI_OBJECT_MAP_H

#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_error.h"

typedef struct {
  DaiObj obj;
  struct hashmap* map;
} DaiObjMap;
DaiObjError*
DaiObjMap_New(DaiVM* vm, const DaiValue* values, int length, DaiObjMap** map_ret);
// 释放 map 对象
void
DaiObjMap_Free(DaiVM* vm, DaiObjMap* map);
/**
 * @brief 迭代 map 中的 kv 对
 *
 * @param map map 对象
 * @param i 迭代计数，用来传给 hashmap_iter ，初始值为 0
 * @param key 用于存储 key
 * @param value 用于存储 value
 *
 * @return true 拿到一个 kv 对，false 迭代结束
 */
bool
DaiObjMap_iter(DaiObjMap* map, size_t* i, DaiValue* key, DaiValue* value);
void
DaiObjMap_cset(DaiObjMap* map, DaiValue key, DaiValue value);
bool
DaiObjMap_cget(DaiObjMap* map, DaiValue key, DaiValue* value);

typedef struct {
  DaiObj obj;
  size_t map_index;   // hashmap_iter 的计数
  DaiObjMap* map;
} DaiObjMapIterator;
DaiObjMapIterator*
DaiObjMapIterator_New(DaiVM* vm, DaiObjMap* map);

#endif /* CBDAI_DAI_OBJECT_MAP_H */

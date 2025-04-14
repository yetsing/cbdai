/*
内存操作封装，用于编译和运行时
*/
#ifndef DF841974_3A3C_41A3_AA7A_2B86D8FE982D
#define DF841974_3A3C_41A3_AA7A_2B86D8FE982D

#include "dai_common.h"
#include "dai_object.h"
#include "dai_vm.h"

// #region vm_reallocate
#define VM_ALLOCATE(vm, type, count) (type*)vm_reallocate(vm, NULL, 0, sizeof(type) * (count))

#define VM_FREE(vm, type, pointer) vm_reallocate(vm, pointer, sizeof(type), 0)
#define VM_FREE1(vm, size, pointer) vm_reallocate(vm, pointer, size, 0)

#define VM_GROW_ARRAY(vm, type, pointer, oldCount, newCount) \
    (type*)vm_reallocate(vm, pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define VM_FREE_ARRAY(vm, type, pointer, oldCount) \
    vm_reallocate(vm, pointer, sizeof(type) * (oldCount), 0)

void*
vm_reallocate(DaiVM* vm, void* pointer, size_t old_size, size_t new_size);

// #endregion

// #region reallocate
#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)

// old_size	new_size	                Operation
// 0	    Non‑zero	            Allocate new block. 分配新块
// Non‑zero	0	                    Free allocation. 释放已分配内存
// Non‑zero	Smaller than old_size	Shrink existing allocation. 收缩已分配内存
// Non‑zero	Larger than old_size    Grow existing allocation. 增加已分配内存
void*
reallocate(void* pointer, size_t oldSize, size_t newSize);

// #endregion

void
markObject(DaiVM* vm, DaiObj* object);
void
markValue(DaiVM* vm, DaiValue value);
void
collectGarbage(DaiVM* vm);

void
dai_free_objects(DaiVM* vm);

#ifdef DAI_TEST
void
test_mark(DaiVM* vm);
#endif
#endif /* DF841974_3A3C_41A3_AA7A_2B86D8FE982D */

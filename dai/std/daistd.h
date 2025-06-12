#ifndef CBDAI_DAISTD_H
#define CBDAI_DAISTD_H

#include "dai_vm.h"

// 标准库的初始化和清理函数

// @brief
// Initializes the standard library for the given DaiVM instance.
// This function will be called before running any Dai code,
// and it should set up the necessary standard library functions and objects.
// @param vm The DaiVM instance to initialize the standard library for.
// @return Returns true if the initialization was successful, false otherwise.
// @note
// This function may allocate resources that need to be cleaned up in quit function.
bool
daistd_init(DaiVM* vm);

// @brief
// Cleans up the standard library for the given DaiVM instance.
// This function will be called after all Dai code has been executed,
// and it should release any resources allocated during initialization.
// @param vm The DaiVM instance to clean up the standard library for.
// @note
// This function should ensure that all resources allocated by daistd_init are properly released.
void
daistd_quit(DaiVM* vm);

#endif /* CBDAI_DAISTD_H */
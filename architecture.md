# 虚拟机

采用的栈式虚拟机，在运算的时候，会将操作数从栈顶弹出，将结果压入栈。

```
比如计算 2 + 3
先将 2 3 压入栈中
[2] [3]
执行加法运算，将 2 3 弹出，将结果 5 压入栈中
[5]
```

## 局部变量

局部变量空间是分配在虚拟机的求值栈上，在调用函数/模块时，会更新求值栈栈顶，给局部变量预分配空间。

之所以用这种方式，是为了在函数调用时不用复制函数参数，减少函数调用开销。

```
比如 foo(1, 2) 函数调用，栈如下所示
[foo] [1] [2]
可以看到函数参数按顺序排列，刚好是第 1 2 ... 个局部变量，可以直接作为局部变量空间
```

预分配的空间在编译时确定（ DaiCompiler.max_local_count ），会记录在函数/模块对象中（ DaiObjFunction.max_local_count DaiObjModule.max_local_count ）。

ref: https://www.craftinginterpreters.com/calls-and-functions.html

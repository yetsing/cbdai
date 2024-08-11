#ifndef B0D14535_3DCF_4F93_879B_8B763471BC56
#define B0D14535_3DCF_4F93_879B_8B763471BC56

// 定义颜色宏
#define RED "\033[0;31m"
#define RESET "\033[0m"
#define GREEN "\033[0;32m"
#define BLUE "\033[0;34m"
#define YELLOW "\033[0;33m"

#define BRACE_COLOR(text) "\033[0;34m" text "\033[0m"
#define KEY_COLOR(text) "\033[0;35m" text "\033[0m"
#define TYPE_COLOR(text) "\033[0;36m" text "\033[0m"

__attribute__ ((unused))
static const char *indent = "    ";
__attribute__ ((unused))
static const char *doubleindent = "        ";

#endif /* B0D14535_3DCF_4F93_879B_8B763471BC56 */

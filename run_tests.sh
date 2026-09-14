#!/bin/sh
# 编译并运行算法验证：拿原站 31 天数据回代，看结果是否一致。
cd "$(dirname "$0")/algo" || exit 1
cc -O2 -Wall -Wextra -o test_dressing dressing.c test_dressing.c || exit 1
exec ./test_dressing

#ifndef SN_COMMON_H
#define SN_COMMON_H

#define CREATE_STACK_FORMATTED_STR(VAR, SIZE, FMT, ...)                                            \
  char VAR[SIZE];                                                                                  \
  snprintf(VAR, SIZE, TOPIC_REGISTER_ACK_FMT, ##__VA_ARGS__)

#define MIN(x, y)      ((x) > (y)) ? (y) : (x)
#define MAX(x, y)      ((x) > (y)) ? (x) : (y)
#define CLAMP(x, a, b) ((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x))

#endif // !SN_COMMON_H

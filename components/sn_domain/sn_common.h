#ifndef SN_COMMON_H
#define SN_COMMON_H

#define CREATE_STACK_FORMATTED_STR(VAR, SIZE, FMT, ...)                                            \
  char VAR[SIZE];                                                                                  \
  snprintf(VAR, SIZE, TOPIC_REGISTER_ACK_FMT, ##__VA_ARGS__)

#endif // !SN_COMMON_H

/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_WORKOUT_PARSER_H_
#define BASEWORK_WORKOUT_PARSER_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif
struct workout_domain;

int workout_parse(const char *content, size_t size, struct workout_domain **p);
int workout_load_files(const char *dir);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_WORKOUT_PARSER_H_ */

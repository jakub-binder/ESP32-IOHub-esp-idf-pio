#ifndef FIXTURE_H
#define FIXTURE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *name;
} fixture_info_t;

void fixture_setup(void);
void fixture_loop(void);
const fixture_info_t *fixture_get_info(void);

#ifdef __cplusplus
}
#endif

#endif /* FIXTURE_H */
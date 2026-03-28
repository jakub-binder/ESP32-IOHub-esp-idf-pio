#ifndef FIXTURE_DEFAULT_H
#define FIXTURE_DEFAULT_H

#include "fixtures/fixture.h"

#ifdef __cplusplus
extern "C" {
#endif

void fixture_default_setup(void);
void fixture_default_loop(void);
const fixture_info_t *fixture_default_get_info(void);

#ifdef __cplusplus
}
#endif

#endif /* FIXTURE_DEFAULT_H */
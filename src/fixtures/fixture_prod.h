#ifndef FIXTURE_PROD_H
#define FIXTURE_PROD_H

#include "fixtures/fixture.h"

#ifdef __cplusplus
extern "C" {
#endif

void fixture_prod_setup(void);
void fixture_prod_loop(void);
const fixture_info_t *fixture_prod_get_info(void);

#ifdef __cplusplus
}
#endif

#endif /* FIXTURE_PROD_H */
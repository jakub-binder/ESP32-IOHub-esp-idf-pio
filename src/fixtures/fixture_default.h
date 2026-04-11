#ifndef FIXTURE_DEFAULT_H
#define FIXTURE_DEFAULT_H

#include "fixtures/fixture.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const fixture_t fixture_default;
const fixture_info_t *fixture_default_get_info(void);

#ifdef __cplusplus
}
#endif

#endif /* FIXTURE_DEFAULT_H */

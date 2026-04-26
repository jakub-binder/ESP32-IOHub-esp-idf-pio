#include <stddef.h>

#include "app_config.h"
#include "fixtures/fixture.h"

#if defined(FIXTURE_DEFAULT)
#include "fixtures/fixture_default.h"
#elif defined(FIXTURE_PROD)
#include "fixtures/fixture_prod.h"
#elif defined(FIXTURE_SCANFIL_SAFETY)
#include "fixtures/fixture_scanfil_safety.h"
#else
#error "No fixture selected"
#endif

const fixture_t *fixture_select(void)
{
#if defined(FIXTURE_DEFAULT)
    return &fixture_default;
#elif defined(FIXTURE_PROD)
    return &fixture_prod;
#elif defined(FIXTURE_SCANFIL_SAFETY)
    return &fixture_scanfil_safety;
#else
    return NULL;
#endif
}

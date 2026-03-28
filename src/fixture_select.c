#include "app_config.h"
#include "fixtures/fixture.h"

#if defined(FIXTURE_DEFAULT)
#include "fixtures/fixture_default.h"
#elif defined(FIXTURE_PROD)
#include "fixtures/fixture_prod.h"
#else
#error "No fixture selected"
#endif

const fixture_info_t *fixture_get_info(void)
{
#if defined(FIXTURE_DEFAULT)
    return fixture_default_get_info();
#elif defined(FIXTURE_PROD)
    return fixture_prod_get_info();
#else
    return 0;
#endif
}

void fixture_setup(void)
{
#if defined(FIXTURE_DEFAULT)
    fixture_default_setup();
#elif defined(FIXTURE_PROD)
    fixture_prod_setup();
#endif
}

void fixture_loop(void)
{
#if defined(FIXTURE_DEFAULT)
    fixture_default_loop();
#elif defined(FIXTURE_PROD)
    fixture_prod_loop();
#endif
}
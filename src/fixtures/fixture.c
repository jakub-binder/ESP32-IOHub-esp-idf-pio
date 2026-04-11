#include "fixtures/fixture.h"

const fixture_t *fixture_select(void);

static const fixture_t *s_selected = NULL;
static fixture_info_t s_fixture_info =
{
    .name = NULL
};

const fixture_t *fixture_get_selected(void)
{
    if (s_selected == NULL)
    {
        s_selected = fixture_select();
    }

    return s_selected;
}

void fixture_setup(void)
{
    const fixture_t *fx = fixture_get_selected();

    if (fx != NULL && fx->setup != NULL)
    {
        fx->setup();
    }

    if (fx != NULL && fx->register_commands != NULL)
    {
        fx->register_commands();
    }
}

void fixture_loop(void)
{
    const fixture_t *fx = fixture_get_selected();

    if (fx != NULL && fx->loop != NULL)
    {
        fx->loop();
    }
}

const fixture_info_t *fixture_get_info(void)
{
    const fixture_t *fx = fixture_get_selected();

    if (fx != NULL)
    {
        s_fixture_info.name = fx->name;
    }

    return &s_fixture_info;
}

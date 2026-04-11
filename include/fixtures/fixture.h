#ifndef FIXTURE_H
#define FIXTURE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *name;
} fixture_info_t;

typedef void (*fixture_setup_fn)(void);
typedef void (*fixture_loop_fn)(void);
typedef void (*fixture_register_commands_fn)(void);

typedef struct fixture
{
    const char *name;
    fixture_setup_fn setup;
    fixture_loop_fn loop;
    fixture_register_commands_fn register_commands;
} fixture_t;

const fixture_t *fixture_get_selected(void);
void fixture_setup(void);
void fixture_loop(void);
const fixture_info_t *fixture_get_info(void);

#ifdef __cplusplus
}
#endif

#endif /* FIXTURE_H */

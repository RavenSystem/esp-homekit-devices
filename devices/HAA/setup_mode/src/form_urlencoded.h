#pragma once

typedef struct _form_param {
    char *name;
    char *value;

    struct _form_param *next;
} form_param_t;

form_param_t *form_params_parse(const char *s);
form_param_t *form_params_find(form_param_t *params, const char *name);
void form_params_free(form_param_t *params);


#ifndef _GOOM_CONFIG_PARAM_H
#define _GOOM_CONFIG_PARAM_H

#include <stdlib.h>

/**
 * File created on 2003-05-24 by Jeko.
 * (c)2003, JC Hoelt for iOS-software.
 *
 * LGPL Licence.
 */

typedef enum {
  PARAM_INTVAL,
  PARAM_FLOATVAL,
  PARAM_BOOLVAL,
  PARAM_STRVAL,
  PARAM_LISTVAL,
} ParamType;

struct IntVal {
  int value;
  int min;
  int max;
  int step;
};
struct FloatVal {
  float value;
  float min;
  float max;
  float step;
};
struct StrVal {
  char *value;
};
struct ListVal {
  char *value;
  int nbChoices;
  char **choices;
};
struct BoolVal {
  int value;
};


typedef struct _PARAM {
  char *name;
  char *desc;
  char rw;
  ParamType type;
  union {
    struct IntVal ival;
    struct FloatVal fval;
    struct StrVal sval;
    struct ListVal slist;
    struct BoolVal bval;
  } param;
  
  /* used by the core to inform the GUI of a change */
  void (*change_listener)(struct _PARAM *_this);

  /* used by the GUI to inform the core of a change */
  void (*changed)(struct _PARAM *_this);
  
  void *user_data; /* can be used by the GUI */
} PluginParam;

#define IVAL(p) ((p).param.ival.value)
#define SVAL(p) ((p).param.sval.value)
#define FVAL(p) ((p).param.fval.value)
#define BVAL(p) ((p).param.bval.value)
#define LVAL(p) ((p).param.slist.value)

#define FMIN(p) ((p).param.fval.min)
#define FMAX(p) ((p).param.fval.max)
#define FSTEP(p) ((p).param.fval.step)

#define IMIN(p) ((p).param.ival.min)
#define IMAX(p) ((p).param.ival.max)
#define ISTEP(p) ((p).param.ival.step)

void empty_fct();

PluginParam secure_param();

PluginParam secure_f_param(char *name);
PluginParam secure_i_param(char *name);
PluginParam secure_b_param(char *name, int value);
PluginParam secure_s_param(char *name);

PluginParam secure_f_feedback(char *name);
PluginParam secure_i_feedback(char *name);

void set_str_param_value(PluginParam *p, const char *str);
void set_list_param_value(PluginParam *p, const char *str);
    
typedef struct _PARAMETERS {
  char *name;
  char *desc;
  int nbParams;
  PluginParam **params;
} PluginParameters;

PluginParameters plugin_parameters(const char *name, int nb);

#endif

/***
Copyright © 2021 gavine99 https://github.com/gavine99

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/xmalloc.h>

#include <pulsecore/core.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/source.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/namereg.h>
#include <pulsecore/core-util.h>
#include <pulsecore/idxset.h>

#define LMPHS_PA_PROP_SINK_ASSOCIATED_MODIDX "X-load-module-per-hardware-sink-assoc-module"
#define LMPHS_PA_PROP_MODULE_ASSOCIATED_SINKIDX "X-load-module-per-hardware-sink-assoc-sink"

PA_MODULE_AUTHOR("gavine99");
PA_MODULE_DESCRIPTION("When a sink is added move sink input streams to it");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "module=<module module to load>, "
        "module_args=<arguments to pass to module module>, use %m to insert new sink name, "
        "switch_on_connect=<bool> switch to new modules associated sink when created"
);

static const char* const valid_modargs[] = {
    "module",
    "module_args",
    "switch_on_connect",
    "steal_default",
    NULL,
};

struct userdata {
    const char* module;
    const char* module_args;
    bool switch_on_connect;
    bool steal_default;
};

pa_module* load_module(pa_core* c, pa_sink* master_sink, const char* module, const char* args, struct userdata* u);

pa_module* load_module(pa_core* c, pa_sink* master_sink, const char* module, const char* args, struct userdata* u) {
    pa_module* new_module;
    char* cooked_args;

    // insert master name in args string
    cooked_args = pa_replace(args, "%m", master_sink->name);

    if (pa_module_load(&new_module, c, module, cooked_args) != PA_OK) {
        pa_log_error("Failed to load module \"%s\" (argument: \"%s\")", module, cooked_args);
        new_module = NULL;
    }

    pa_xfree(cooked_args);

    return new_module;
}

static pa_hook_result_t sink_put_hook_callback(pa_core *c, pa_sink *new_sink, void* userdata) {
    struct userdata *u = userdata;
    pa_module* new_module;
    pa_sink* sink;
    uint32_t idx;

    pa_assert(c);
    pa_assert(new_sink);
    pa_assert(userdata);

    pa_log_debug("New sink has been put \"%s\"", new_sink->name);

    // only create new module for hardware sinks
    if (!(new_sink->flags & PA_SINK_HARDWARE)) {
        pa_log_debug("New sink \"%s\" is not a hardware sink. Ignoring", new_sink->name);
        return PA_HOOK_OK;
    }

    pa_log_info("Loading module \"%s\" for new sink \"%s\"", u->module, new_sink->name);

    if ((new_module = load_module(c, new_sink, u->module, u->module_args, u)) == NULL)
        return PA_HOOK_OK;

    pa_proplist_setf(new_sink->proplist, LMPHS_PA_PROP_SINK_ASSOCIATED_MODIDX, "%d", new_module->index);
    pa_proplist_setf(new_module->proplist, LMPHS_PA_PROP_MODULE_ASSOCIATED_SINKIDX, "%d", new_sink->index);

    if (u->switch_on_connect == false)
        return PA_HOOK_OK;

    // iterate all sinks
    PA_IDXSET_FOREACH(sink, c->sinks, idx) {
        // if sink is not owned by new module, skip it
        if (sink->module->index != new_module->index)
            continue;

        // set sink as default
        pa_log_info("Setting sink \"%s\" as new default sink", sink->name);
        pa_core_set_configured_default_sink(c, sink->name);

        return PA_HOOK_OK;
    }

    pa_log_info("The module just loaded doesn't have it's own sink the default sink was not changed. That's ok.");

    return PA_HOOK_OK;
}

static pa_hook_result_t default_sink_changed_callback(pa_core *c, pa_sink *new_sink, void* userdata) {
    struct userdata *u = userdata;
    const char* ami_string;
    uint32_t assoc_module_index;
    pa_sink* sink;
    uint32_t idx;

    pa_assert(c);
    pa_assert(new_sink);
    pa_assert(userdata);

    if (u->steal_default == false)
        return PA_HOOK_OK;

    ami_string = pa_proplist_gets(new_sink->proplist, LMPHS_PA_PROP_SINK_ASSOCIATED_MODIDX);
    if (ami_string == NULL) {
        pa_log_debug("New default sink \"%s\" was not assoicated with a module loaded by this module", new_sink->name);
        return PA_HOOK_OK;
    }

    if (pa_atou(ami_string, &assoc_module_index) != 0)
        return PA_HOOK_OK;

    pa_log_debug("New default sink \"%s\" is associated with module index %d which was loaded by this module", new_sink->name, assoc_module_index);

    // iterate all sinks to find first of associated modules sink's (if it exists)
    PA_IDXSET_FOREACH(sink, c->sinks, idx) {
        // if sink is not owned by new module, skip it
        if (sink->module->index != assoc_module_index)
            continue;

        // set sink as default
        pa_log_info("Setting sink \"%s\" as new default sink", sink->name);
        pa_core_set_configured_default_sink(c, sink->name);

        return PA_HOOK_OK;
    }

    pa_log_info("The module loaded for the default sink doesn't have it's own sink so the default sink wasn't 'stolen' by us. That's ok.");

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma;
    struct userdata *u;
    uint32_t idx;
    pa_sink* sink;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        return -1;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);

    u->module = pa_xstrdup(pa_modargs_get_value(ma, "module", ""));
    if ((u->module == NULL) || (*u->module == '\0')) {
        pa_log("Failed to parse module_mdoule value or it was empty");
        goto fail;
    }

    u->module_args = pa_xstrdup(pa_modargs_get_value(ma, "module_args", ""));
    if ((u->module_args == NULL) || (*u->module_args == '\0')) {
        pa_log("Failed to parse module_args value or it was empty");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "switch_on_connect", &u->switch_on_connect) < 0) {
        pa_log("Failed to get a boolean value for switch_on_connect.");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "steal_default", &u->steal_default) < 0) {
        pa_log("Failed to get a boolean value for steal_default.");
        goto fail;
    }

    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE+30, (pa_hook_cb_t)sink_put_hook_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_DEFAULT_SINK_CHANGED], PA_HOOK_LATE+30, (pa_hook_cb_t)default_sink_changed_callback, u);

    // iterate over existing sinks and create a module for them
    PA_IDXSET_FOREACH(sink, m->core->sinks, idx) {
        if (!PA_SINK_IS_LINKED(sink->state))
            continue;

        // do exactly what callback does
        sink_put_hook_callback(m->core, sink, u);
    }

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    pa_xfree((void*)u->module);
    pa_xfree((void*)u->module_args);

    pa_xfree((void*)u);
}

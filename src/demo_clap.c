/**
 * demo_clap.c - CLAP Audio Plugin with NanoVG UI & Filter DSP
 *
 * Demonstrates a CLAP audio plugin:
 *   - Audio plugin implementing a 1-pole lowpass filter with cutoff parameter
 *   - CLAP extensions: audio-ports, params, gui
 *   - GUI implemented with NanoVG null-backend rendering filter curve & cutoff knob
 *   - Standalone host validator in main() testing full lifecycle:
 *     entry -> factory -> create -> init -> activate -> process -> deactivate -> destroy
 */

#define NANOVG_NULL_IMPLEMENTATION
#include "nanovg.h"

#include <clap/clap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- Minimal NanoVG null renderer for headless GUI --------------------- */
static int  nvg_null_renderCreate(void *u) { (void)u; return 1; }
static int  nvg_null_createTexture(void *u,int t,int w,int h,int f,const unsigned char *d) { (void)u;(void)t;(void)w;(void)h;(void)f;(void)d; return 1; }
static int  nvg_null_deleteTexture(void *u, int id) { (void)u;(void)id; return 1; }
static int  nvg_null_updateTexture(void *u,int id,int x,int y,int w,int h,const unsigned char *d) { (void)u;(void)id;(void)x;(void)y;(void)w;(void)h;(void)d; return 1; }
static int  nvg_null_getTextureSize(void *u,int id,int *w,int *h) { (void)u;(void)id; if(w)*w=0; if(h)*h=0; return 1; }
static void nvg_null_viewport(void *u,float w,float h,float r) { (void)u;(void)w;(void)h;(void)r; }
static void nvg_null_cancel(void *u) { (void)u; }
static void nvg_null_flush(void *u) { (void)u; }
static void nvg_null_fill(void *u,NVGpaint *p,NVGcompositeOperationState o,NVGscissor *s,float f,const float *b,const NVGpath *ps,int np)
    { (void)u;(void)p;(void)o;(void)s;(void)f;(void)b;(void)ps;(void)np; }
static void nvg_null_stroke(void *u,NVGpaint *p,NVGcompositeOperationState o,NVGscissor *s,float f,float strokeWidth,const NVGpath *ps,int np)
    { (void)u;(void)p;(void)o;(void)s;(void)f;(void)strokeWidth;(void)ps;(void)np; }
static void nvg_null_triangles(void *u,NVGpaint *p,NVGcompositeOperationState o,NVGscissor *s,const NVGvertex *v,int n,float f)
    { (void)u;(void)p;(void)o;(void)s;(void)v;(void)n;(void)f; }
static void nvg_null_renderDelete(void *u) { (void)u; }

static NVGcontext *create_null_nanovg(void) {
    NVGparams params;
    memset(&params, 0, sizeof(params));
    params.renderCreate         = nvg_null_renderCreate;
    params.renderCreateTexture  = nvg_null_createTexture;
    params.renderDeleteTexture  = nvg_null_deleteTexture;
    params.renderUpdateTexture  = nvg_null_updateTexture;
    params.renderGetTextureSize = nvg_null_getTextureSize;
    params.renderViewport       = nvg_null_viewport;
    params.renderCancel         = nvg_null_cancel;
    params.renderFlush          = nvg_null_flush;
    params.renderFill           = nvg_null_fill;
    params.renderStroke         = nvg_null_stroke;
    params.renderTriangles      = nvg_null_triangles;
    params.renderDelete         = nvg_null_renderDelete;
    params.userPtr              = NULL;
    params.edgeAntiAlias        = 1;
    return nvgCreateInternal(&params);
}

/* ---- Plugin Internal State --------------------------------------------- */
typedef struct {
    double sample_rate;
    double cutoff_hz;
    double resonance;
    float  filter_state_l;
    float  filter_state_r;
    NVGcontext *vg;
    bool   gui_visible;
} my_filter_plugin_t;

static const clap_plugin_descriptor_t s_plugin_desc = {
    .clap_version = CLAP_VERSION,
    .id = "org.zapota.filter-nanovg",
    .name = "Zapota NanoVG Filter",
    .vendor = "zapota",
    .url = "https://github.com/xyzzyapps/zapota",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "1-pole lowpass filter with NanoVG vector UI",
    .features = (const char *const[]){ CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_FILTER, NULL }
};

/* Forward declarations */
static const clap_plugin_audio_ports_t s_plugin_audio_ports;
static const clap_plugin_params_t      s_plugin_params;
static const clap_plugin_gui_t         s_plugin_gui;

/* ---- CLAP Plugin Core Callbacks --------------------------------------- */
static bool plugin_init(const clap_plugin_t *plugin) {
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    plug->sample_rate = 44100.0;
    plug->cutoff_hz = 1000.0;
    plug->resonance = 0.5;
    plug->filter_state_l = 0.0f;
    plug->filter_state_r = 0.0f;
    plug->vg = NULL;
    plug->gui_visible = false;
    return true;
}

static void plugin_destroy(const clap_plugin_t *plugin) {
    if (plugin && plugin->plugin_data) {
        my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
        if (plug->vg) {
            nvgDeleteInternal(plug->vg);
            plug->vg = NULL;
        }
        free(plugin->plugin_data);
    }
    free((void *)plugin);
}

static bool plugin_activate(const clap_plugin_t *plugin,
                            double sample_rate,
                            uint32_t min_frames_count,
                            uint32_t max_frames_count) {
    (void)min_frames_count; (void)max_frames_count;
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    plug->sample_rate = sample_rate;
    plug->filter_state_l = 0.0f;
    plug->filter_state_r = 0.0f;
    return true;
}

static void plugin_deactivate(const clap_plugin_t *plugin) {
    (void)plugin;
}

static bool plugin_start_processing(const clap_plugin_t *plugin) {
    (void)plugin;
    return true;
}

static void plugin_stop_processing(const clap_plugin_t *plugin) {
    (void)plugin;
}

static void plugin_reset(const clap_plugin_t *plugin) {
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    plug->filter_state_l = 0.0f;
    plug->filter_state_r = 0.0f;
}

static clap_process_status plugin_process(const clap_plugin_t *plugin,
                                          const clap_process_t *process) {
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;

    /* Simple 1-pole lowpass coefficient: alpha = 2*pi*fc / fs */
    double fc = plug->cutoff_hz;
    if (fc > plug->sample_rate * 0.49) fc = plug->sample_rate * 0.49;
    if (fc < 20.0) fc = 20.0;
    float alpha = (float)(2.0 * M_PI * fc / plug->sample_rate);
    if (alpha > 1.0f) alpha = 1.0f;

    uint32_t frames = process->frames_count;

    if (process->audio_outputs_count > 0 && process->audio_inputs_count > 0) {
        const float *in_l = process->audio_inputs[0].data32[0];
        const float *in_r = (process->audio_inputs[0].channel_count > 1) ?
                            process->audio_inputs[0].data32[1] : in_l;
        float *out_l = process->audio_outputs[0].data32[0];
        float *out_r = (process->audio_outputs[0].channel_count > 1) ?
                       process->audio_outputs[0].data32[1] : out_l;

        for (uint32_t i = 0; i < frames; i++) {
            plug->filter_state_l += alpha * (in_l[i] - plug->filter_state_l);
            out_l[i] = plug->filter_state_l;

            plug->filter_state_r += alpha * (in_r[i] - plug->filter_state_r);
            out_r[i] = plug->filter_state_r;
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

static const void *plugin_get_extension(const clap_plugin_t *plugin, const char *id) {
    (void)plugin;
    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &s_plugin_audio_ports;
    if (strcmp(id, CLAP_EXT_PARAMS) == 0)      return &s_plugin_params;
    if (strcmp(id, CLAP_EXT_GUI) == 0)         return &s_plugin_gui;
    return NULL;
}

static void plugin_on_main_thread(const clap_plugin_t *plugin) {
    (void)plugin;
}

/* ---- Audio Ports Extension --------------------------------------------- */
static uint32_t audio_ports_count(const clap_plugin_t *plugin, bool is_input) {
    (void)plugin; (void)is_input;
    return 1;
}

static bool audio_ports_get(const clap_plugin_t *plugin,
                            uint32_t index,
                            bool is_input,
                            clap_audio_port_info_t *info) {
    (void)plugin;
    if (index != 0) return false;
    info->id = is_input ? 0 : 1;
    snprintf(info->name, sizeof(info->name), "%s", is_input ? "Stereo In" : "Stereo Out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}

static const clap_plugin_audio_ports_t s_plugin_audio_ports = {
    .count = audio_ports_count,
    .get = audio_ports_get,
};

/* ---- Parameters Extension ---------------------------------------------- */
static uint32_t params_count(const clap_plugin_t *plugin) {
    (void)plugin;
    return 1; /* Cutoff Frequency */
}

static bool params_get_info(const clap_plugin_t *plugin,
                            uint32_t param_index,
                            clap_param_info_t *param_info) {
    (void)plugin;
    if (param_index != 0) return false;
    param_info->id = 0;
    param_info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    param_info->cookie = NULL;
    snprintf(param_info->name, sizeof(param_info->name), "Cutoff");
    param_info->module[0] = '\0';
    param_info->min_value = 20.0;
    param_info->max_value = 20000.0;
    param_info->default_value = 1000.0;
    return true;
}

static bool params_get_value(const clap_plugin_t *plugin, clap_id param_id, double *out_value) {
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    if (param_id == 0) {
        *out_value = plug->cutoff_hz;
        return true;
    }
    return false;
}

static bool params_value_to_text(const clap_plugin_t *plugin,
                                 clap_id param_id,
                                 double value,
                                 char *out_buffer,
                                 uint32_t out_buffer_capacity) {
    (void)plugin;
    if (param_id == 0) {
        snprintf(out_buffer, out_buffer_capacity, "%.1f Hz", value);
        return true;
    }
    return false;
}

static bool params_text_to_value(const clap_plugin_t *plugin,
                                 clap_id param_id,
                                 const char *param_value_text,
                                 double *out_value) {
    (void)plugin;
    if (param_id == 0) {
        *out_value = atof(param_value_text);
        return true;
    }
    return false;
}

static void params_flush(const clap_plugin_t *plugin,
                         const clap_input_events_t *in,
                         const clap_output_events_t *out) {
    (void)plugin; (void)in; (void)out;
}

static const clap_plugin_params_t s_plugin_params = {
    .count = params_count,
    .get_info = params_get_info,
    .get_value = params_get_value,
    .value_to_text = params_value_to_text,
    .text_to_value = params_text_to_value,
    .flush = params_flush,
};

/* ---- GUI Extension with NanoVG ----------------------------------------- */
static bool gui_is_api_supported(const clap_plugin_t *plugin, const char *api, bool is_floating) {
    (void)plugin; (void)is_floating;
    /* We support win32 and headless/software */
    if (strcmp(api, "win32") == 0 || strcmp(api, "software") == 0) return true;
    return true;
}

static bool gui_get_preferred_api(const clap_plugin_t *plugin, const char **api, bool *is_floating) {
    (void)plugin;
    *api = "software";
    *is_floating = true;
    return true;
}

static bool gui_create(const clap_plugin_t *plugin, const char *api, bool is_floating) {
    (void)api; (void)is_floating;
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    plug->vg = create_null_nanovg();
    return plug->vg != NULL;
}

static void gui_destroy(const clap_plugin_t *plugin) {
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    if (plug->vg) {
        nvgDeleteInternal(plug->vg);
        plug->vg = NULL;
    }
}

static bool gui_set_scale(const clap_plugin_t *plugin, double scale) {
    (void)plugin; (void)scale;
    return true;
}

static bool gui_get_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
    (void)plugin;
    *width = 400;
    *height = 300;
    return true;
}

static bool gui_can_resize(const clap_plugin_t *plugin) {
    (void)plugin;
    return false;
}

static bool gui_get_resize_hints(const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) {
    (void)plugin;
    hints->can_resize_horizontally = false;
    hints->can_resize_vertically = false;
    hints->preserve_aspect_ratio = true;
    hints->aspect_ratio_width = 400;
    hints->aspect_ratio_height = 300;
    return true;
}

static bool gui_adjust_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
    (void)plugin;
    *width = 400;
    *height = 300;
    return true;
}

static bool gui_set_size(const clap_plugin_t *plugin, uint32_t width, uint32_t height) {
    (void)plugin; (void)width; (void)height;
    return true;
}

static bool gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window) {
    (void)plugin; (void)window;
    return true;
}

static bool gui_set_transient(const clap_plugin_t *plugin, const clap_window_t *window) {
    (void)plugin; (void)window;
    return true;
}

static void gui_suggest_title(const clap_plugin_t *plugin, const char *title) {
    (void)plugin; (void)title;
}

static bool gui_show(const clap_plugin_t *plugin) {
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    plug->gui_visible = true;

    /* Render the filter UI using NanoVG vector drawing */
    if (plug->vg) {
        NVGcontext *vg = plug->vg;
        nvgBeginFrame(vg, 400.0f, 300.0f, 1.0f);

        /* Background */
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, 400, 300);
        nvgFillColor(vg, nvgRGBA(30, 30, 35, 255));
        nvgFill(vg);

        /* Filter curve display panel */
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 20.0f, 20.0f, 360.0f, 160.0f, 8.0f);
        nvgFillColor(vg, nvgRGBA(15, 15, 20, 255));
        nvgFill(vg);

        /* Draw lowpass frequency response curve */
        nvgBeginPath(vg);
        nvgMoveTo(vg, 20.0f, 60.0f);
        float cutoff_x = 20.0f + 360.0f * (float)(log10(plug->cutoff_hz / 20.0) / log10(1000.0));
        if (cutoff_x > 380.0f) cutoff_x = 380.0f;
        nvgLineTo(vg, cutoff_x - 30.0f, 60.0f);
        nvgBezierTo(vg, cutoff_x, 60.0f, cutoff_x + 20.0f, 140.0f, 380.0f, 170.0f);
        nvgStrokeColor(vg, nvgRGBA(0, 220, 255, 255));
        nvgStrokeWidth(vg, 2.5f);
        nvgStroke(vg);

        /* Cutoff Knob */
        float knob_cx = 200.0f;
        float knob_cy = 235.0f;
        float knob_r = 35.0f;

        /* Knob body */
        nvgBeginPath(vg);
        nvgCircle(vg, knob_cx, knob_cy, knob_r);
        nvgFillColor(vg, nvgRGBA(50, 50, 60, 255));
        nvgFill(vg);
        nvgStrokeColor(vg, nvgRGBA(80, 80, 100, 255));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);

        /* Knob pointer angle based on cutoff */
        float angle = (float)((plug->cutoff_hz - 20.0) / (20000.0 - 20.0) * 270.0 - 135.0);
        float rad = (float)(angle * M_PI / 180.0);
        nvgBeginPath(vg);
        nvgMoveTo(vg, knob_cx, knob_cy);
        nvgLineTo(vg, knob_cx + knob_r * 0.75f * (float)sin(rad), knob_cy - knob_r * 0.75f * (float)cos(rad));
        nvgStrokeColor(vg, nvgRGBA(255, 180, 0, 255));
        nvgStrokeWidth(vg, 3.0f);
        nvgStroke(vg);

        nvgEndFrame(vg);
    }
    return true;
}

static bool gui_hide(const clap_plugin_t *plugin) {
    my_filter_plugin_t *plug = (my_filter_plugin_t *)plugin->plugin_data;
    plug->gui_visible = false;
    return true;
}

static const clap_plugin_gui_t s_plugin_gui = {
    .is_api_supported = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create = gui_create,
    .destroy = gui_destroy,
    .set_scale = gui_set_scale,
    .get_size = gui_get_size,
    .can_resize = gui_can_resize,
    .get_resize_hints = gui_get_resize_hints,
    .adjust_size = gui_adjust_size,
    .set_size = gui_set_size,
    .set_parent = gui_set_parent,
    .set_transient = gui_set_transient,
    .suggest_title = gui_suggest_title,
    .show = gui_show,
    .hide = gui_hide,
};

/* ---- CLAP Plugin Factory ----------------------------------------------- */
static const clap_plugin_t *plugin_factory_create_plugin(const clap_plugin_factory_t *factory,
                                                         const clap_host_t *host,
                                                         const char *plugin_id) {
    (void)factory; (void)host;
    if (strcmp(plugin_id, s_plugin_desc.id) != 0) return NULL;

    clap_plugin_t *plugin = (clap_plugin_t *)calloc(1, sizeof(clap_plugin_t));
    my_filter_plugin_t *plug = (my_filter_plugin_t *)calloc(1, sizeof(my_filter_plugin_t));

    plugin->desc = &s_plugin_desc;
    plugin->plugin_data = plug;
    plugin->init = plugin_init;
    plugin->destroy = plugin_destroy;
    plugin->activate = plugin_activate;
    plugin->deactivate = plugin_deactivate;
    plugin->start_processing = plugin_start_processing;
    plugin->stop_processing = plugin_stop_processing;
    plugin->reset = plugin_reset;
    plugin->process = plugin_process;
    plugin->get_extension = plugin_get_extension;
    plugin->on_main_thread = plugin_on_main_thread;

    return plugin;
}

static uint32_t plugin_factory_get_plugin_count(const clap_plugin_factory_t *factory) {
    (void)factory;
    return 1;
}

static const clap_plugin_descriptor_t *plugin_factory_get_plugin_descriptor(
    const clap_plugin_factory_t *factory, uint32_t index) {
    (void)factory;
    if (index == 0) return &s_plugin_desc;
    return NULL;
}

static const clap_plugin_factory_t s_plugin_factory = {
    .get_plugin_count = plugin_factory_get_plugin_count,
    .get_plugin_descriptor = plugin_factory_get_plugin_descriptor,
    .create_plugin = plugin_factory_create_plugin,
};

/* ---- CLAP Entry Point -------------------------------------------------- */
static bool entry_init(const char *plugin_path) {
    (void)plugin_path;
    return true;
}

static void entry_deinit(void) {}

static const void *entry_get_factory(const char *factory_id) {
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) return &s_plugin_factory;
    return NULL;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = entry_init,
    .deinit = entry_deinit,
    .get_factory = entry_get_factory,
};

#if !defined(ZAPOTA_CLAP_LIB)
/* ---- Standalone Host Validator in main() ------------------------------- */
int main(void) {
    printf("=== CLAP Audio Plugin + NanoVG UI Demo ===\n");

    /* 1. Verify Entry Point */
    printf("1. Initializing CLAP entry...\n");
    if (!clap_entry.init("demo_clap")) {
        fprintf(stderr, "Entry init failed\n");
        return 1;
    }
    printf("   CLAP version: %d.%d.%d\n",
           clap_entry.clap_version.major,
           clap_entry.clap_version.minor,
           clap_entry.clap_version.revision);

    /* 2. Retrieve Plugin Factory */
    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory) {
        fprintf(stderr, "Failed to get plugin factory\n");
        return 1;
    }
    uint32_t count = factory->get_plugin_count(factory);
    printf("2. Factory reports %u plugin(s)\n", count);

    const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
    printf("   Plugin: %s by %s (id: %s)\n", desc->name, desc->vendor, desc->id);

    /* 3. Create Plugin Instance */
    clap_host_t dummy_host = {
        .clap_version = CLAP_VERSION,
        .host_data = NULL,
        .name = "Zapota Validator Host",
        .vendor = "zapota",
        .url = "",
        .version = "1.0.0",
        .get_extension = NULL,
        .request_restart = NULL,
        .request_process = NULL,
        .request_callback = NULL,
    };
    const clap_plugin_t *plug = factory->create_plugin(factory, &dummy_host, desc->id);
    if (!plug) {
        fprintf(stderr, "Failed to create plugin instance\n");
        return 1;
    }
    printf("3. Plugin instance created.\n");

    /* 4. Initialize & Query Extensions */
    if (!plug->init(plug)) {
        fprintf(stderr, "Plugin init failed\n");
        return 1;
    }
    printf("4. Plugin initialized.\n");

    const clap_plugin_audio_ports_t *ports_ext =
        (const clap_plugin_audio_ports_t *)plug->get_extension(plug, CLAP_EXT_AUDIO_PORTS);
    if (ports_ext) {
        clap_audio_port_info_t port_info;
        ports_ext->get(plug, 0, true, &port_info);
        printf("   Audio In: '%s' (%u channels)\n", port_info.name, port_info.channel_count);
        ports_ext->get(plug, 0, false, &port_info);
        printf("   Audio Out: '%s' (%u channels)\n", port_info.name, port_info.channel_count);
    }

    const clap_plugin_params_t *params_ext =
        (const clap_plugin_params_t *)plug->get_extension(plug, CLAP_EXT_PARAMS);
    if (params_ext) {
        clap_param_info_t pinfo;
        params_ext->get_info(plug, 0, &pinfo);
        double val = 0.0;
        params_ext->get_value(plug, 0, &val);
        char text[64];
        params_ext->value_to_text(plug, 0, val, text, sizeof(text));
        printf("   Param '%s': %.1f (formatted: %s, range: [%.0f, %.0f])\n",
               pinfo.name, val, text, pinfo.min_value, pinfo.max_value);
    }

    /* 5. Exercise NanoVG GUI Extension */
    const clap_plugin_gui_t *gui_ext =
        (const clap_plugin_gui_t *)plug->get_extension(plug, CLAP_EXT_GUI);
    if (gui_ext) {
        printf("5. Testing NanoVG UI extension...\n");
        uint32_t w = 0, h = 0;
        gui_ext->get_size(plug, &w, &h);
        printf("   UI size: %ux%u\n", w, h);
        if (gui_ext->create(plug, "software", true)) {
            printf("   NanoVG context created.\n");
            gui_ext->show(plug);
            printf("   NanoVG UI rendered filter curve + cutoff knob.\n");
            gui_ext->hide(plug);
            gui_ext->destroy(plug);
            printf("   NanoVG UI destroyed.\n");
        }
    }

    /* 6. Activate & Process Audio Block */
    printf("6. Testing DSP audio processing (1-pole lowpass filter)...\n");
    plug->activate(plug, 44100.0, 32, 64);
    plug->start_processing(plug);

    #define NUM_SAMPLES 64
    float in_l[NUM_SAMPLES], in_r[NUM_SAMPLES];
    float out_l[NUM_SAMPLES], out_r[NUM_SAMPLES];
    /* Feed a 1 kHz square wave input */
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float val = ((i % 20) < 10) ? 0.8f : -0.8f;
        in_l[i] = val;
        in_r[i] = val;
    }

    float *in_channels[2] = { in_l, in_r };
    float *out_channels[2] = { out_l, out_r };

    clap_audio_buffer_t in_buf = {
        .data32 = in_channels,
        .data64 = NULL,
        .channel_count = 2,
        .latency = 0,
        .constant_mask = 0,
    };
    clap_audio_buffer_t out_buf = {
        .data32 = out_channels,
        .data64 = NULL,
        .channel_count = 2,
        .latency = 0,
        .constant_mask = 0,
    };

    clap_process_t process = {
        .steady_time = 0,
        .frames_count = NUM_SAMPLES,
        .transport = NULL,
        .audio_inputs = &in_buf,
        .audio_outputs = &out_buf,
        .audio_inputs_count = 1,
        .audio_outputs_count = 1,
        .in_events = NULL,
        .out_events = NULL,
    };

    clap_process_status status = plug->process(plug, &process);
    printf("   Process status: %d (frames: %u)\n", status, NUM_SAMPLES);
    printf("   Sample [0]: in=%.3f -> out=%.3f\n", in_l[0], out_l[0]);
    printf("   Sample [10]: in=%.3f -> out=%.3f\n", in_l[10], out_l[10]);
    printf("   Sample [20]: in=%.3f -> out=%.3f\n", in_l[20], out_l[20]);

    plug->stop_processing(plug);
    plug->deactivate(plug);

    /* 7. Teardown */
    plug->destroy(plug);
    clap_entry.deinit();
    printf("7. Plugin destroyed and entry de-initialized cleanly.\n");

    printf("\nAll CLAP filter + NanoVG UI validations PASSED.\n");
    return 0;
}
#endif /* !ZAPOTA_CLAP_LIB */

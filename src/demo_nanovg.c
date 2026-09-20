/**
 * demo_nanovg.c - NanoVG vector graphics (headless / software) demo
 *
 * NanoVG normally targets OpenGL but exposes a pure C public API for
 * path construction, transforms, and paint creation that can be exercised
 * without a GPU.  This demo creates a null/memory context, calls the
 * geometry & paint building API, and reports the results to stdout.
 *
 * The NANOVG_NULL_IMPLEMENTATION backend is provided inline here via a
 * minimal stub renderer so no OpenGL or window system is required.
 */

#define NANOVG_NULL_IMPLEMENTATION
#include "nanovg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- Minimal null renderer -------------------------------------------- */
/* NanoVG calls a function table (NVGparams) that drives the GPU backend.
   We provide a do-nothing implementation so the C API can be exercised. */

static int  nvg_null_renderCreate(void *udata)                          { (void)udata; return 1; }
static int  nvg_null_createTexture(void *u,int t,int w,int h,int f,const unsigned char *d)
                                                                         { (void)u;(void)t;(void)w;(void)h;(void)f;(void)d; return 1; }
static int  nvg_null_deleteTexture(void *u, int id)                      { (void)u;(void)id; return 1; }
static int  nvg_null_updateTexture(void *u,int id,int x,int y,int w,int h,const unsigned char *d)
                                                                         { (void)u;(void)id;(void)x;(void)y;(void)w;(void)h;(void)d; return 1; }
static int  nvg_null_getTextureSize(void *u,int id,int *w,int *h)       { (void)u;(void)id; if(w)*w=0; if(h)*h=0; return 1; }
static void nvg_null_viewport(void *u,float w,float h,float r)          { (void)u;(void)w;(void)h;(void)r; }
static void nvg_null_cancel(void *u)                                     { (void)u; }
static void nvg_null_flush(void *u)                                      { (void)u; }
static void nvg_null_fill(void *u,NVGpaint *p,NVGcompositeOperationState o,NVGscissor *s,float f,const float *b,const NVGpath *ps,int np)
                                                                         { (void)u;(void)p;(void)o;(void)s;(void)f;(void)b;(void)ps;(void)np; }
static void nvg_null_stroke(void *u,NVGpaint *p,NVGcompositeOperationState o,NVGscissor *s,float f,float strokeWidth,const NVGpath *ps,int np)
                                                                         { (void)u;(void)p;(void)o;(void)s;(void)f;(void)strokeWidth;(void)ps;(void)np; }
static void nvg_null_triangles(void *u,NVGpaint *p,NVGcompositeOperationState o,NVGscissor *s,const NVGvertex *v,int n,float f)
                                                                         { (void)u;(void)p;(void)o;(void)s;(void)v;(void)n;(void)f; }
static void nvg_null_renderDelete(void *u)                               { (void)u; }

/** Create a NanoVG context backed by the null renderer. */
static NVGcontext *nvgCreateNull(void) {
    NVGparams params;
    memset(&params, 0, sizeof(params));
    params.renderCreate      = nvg_null_renderCreate;
    params.renderCreateTexture = nvg_null_createTexture;
    params.renderDeleteTexture = nvg_null_deleteTexture;
    params.renderUpdateTexture = nvg_null_updateTexture;
    params.renderGetTextureSize = nvg_null_getTextureSize;
    params.renderViewport    = nvg_null_viewport;
    params.renderCancel      = nvg_null_cancel;
    params.renderFlush       = nvg_null_flush;
    params.renderFill        = nvg_null_fill;
    params.renderStroke      = nvg_null_stroke;
    params.renderTriangles   = nvg_null_triangles;
    params.renderDelete      = nvg_null_renderDelete;
    params.userPtr           = NULL;
    params.edgeAntiAlias     = 1;
    return nvgCreateInternal(&params);
}

static void nvgDeleteNull(NVGcontext *ctx) {
    nvgDeleteInternal(ctx);
}

/* ----------------------------------------------------------------------- */

int main(void) {
    printf("=== NanoVG Headless Demo ===\n");

    NVGcontext *vg = nvgCreateNull();
    if (!vg) {
        fprintf(stderr, "Failed to create NanoVG context\n");
        return 1;
    }

    /* Simulate a 800x600 frame */
    nvgBeginFrame(vg, 800.0f, 600.0f, 1.0f);

    /* Draw a rounded rectangle path */
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 50.0f, 50.0f, 300.0f, 200.0f, 20.0f);
    nvgFillColor(vg, nvgRGBA(100, 180, 255, 220));
    nvgFill(vg);
    printf("  Drew rounded rectangle (50,50) 300x200 r=20\n");

    /* Draw a circle */
    nvgBeginPath(vg);
    nvgCircle(vg, 400.0f, 300.0f, 80.0f);
    nvgStrokeColor(vg, nvgRGBA(255, 100, 50, 255));
    nvgStrokeWidth(vg, 4.0f);
    nvgStroke(vg);
    printf("  Drew circle at (400,300) r=80\n");

    /* Draw an arc */
    nvgBeginPath(vg);
    nvgArc(vg, 600.0f, 150.0f, 60.0f, nvgDegToRad(0), nvgDegToRad(270), NVG_CW);
    nvgFillColor(vg, nvgRGBA(80, 220, 80, 200));
    nvgFill(vg);
    printf("  Drew arc at (600,150) r=60 270-degrees\n");

    /* Linear gradient paint */
    NVGpaint grad = nvgLinearGradient(vg, 100.0f, 400.0f, 400.0f, 400.0f,
                                       nvgRGBA(255, 255, 0, 255),
                                       nvgRGBA(255, 0, 255, 255));
    nvgBeginPath(vg);
    nvgRect(vg, 100.0f, 400.0f, 300.0f, 80.0f);
    nvgFillPaint(vg, grad);
    nvgFill(vg);
    printf("  Drew gradient rectangle (100,400) 300x80\n");

    /* Transform demonstration */
    nvgSave(vg);
    nvgTranslate(vg, 650.0f, 450.0f);
    nvgRotate(vg, nvgDegToRad(45.0f));
    nvgScale(vg, 1.5f, 0.8f);
    nvgBeginPath(vg);
    nvgRect(vg, -40.0f, -25.0f, 80.0f, 50.0f);
    nvgFillColor(vg, nvgRGBA(200, 50, 200, 180));
    nvgFill(vg);
    nvgRestore(vg);
    printf("  Drew transformed (translate+rotate+scale) rectangle\n");

    nvgEndFrame(vg);

    printf("\nFrame rendered headlessly (null backend, no GPU required).\n");

    nvgDeleteNull(vg);

    printf("NanoVG demo complete.\n");
    return 0;
}

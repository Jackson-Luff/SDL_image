/*
  SDL_image:  An example image loading library for use with SDL
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* This is a JPEG image file loading framework */

#include <SDL3_image/SDL_image.h>

#include <stdio.h>
#include <setjmp.h>


/* We'll have JPG save support by default */
#ifndef SDL_IMAGE_SAVE_JPG
#define SDL_IMAGE_SAVE_JPG    1
#endif

#if defined(USE_STBIMAGE)
#undef WANT_JPEGLIB
#elif defined(SDL_IMAGE_USE_COMMON_BACKEND)
#define WANT_JPEGLIB
#elif defined(SDL_IMAGE_USE_WIC_BACKEND)
#undef WANT_JPEGLIB
#elif defined(__APPLE__) && defined(JPG_USES_IMAGEIO)
#undef WANT_JPEGLIB
#else
#define WANT_JPEGLIB
#endif

#ifdef LOAD_JPG

#ifdef WANT_JPEGLIB

#define USE_JPEGLIB

#include <jpeglib.h>

#ifdef JPEG_TRUE  /* MinGW version of jpeg-8.x renamed TRUE to JPEG_TRUE etc. */
    typedef JPEG_boolean boolean;
    #define TRUE JPEG_TRUE
    #define FALSE JPEG_FALSE
#endif

/* Define this for fast loading and not as good image quality */
/*#define FAST_JPEG*/

/* Define this for quicker (but less perfect) JPEG identification */
#define FAST_IS_JPEG

static struct {
    int loaded;
    void *handle;
    void (*jpeg_calc_output_dimensions) (j_decompress_ptr cinfo);
    void (*jpeg_CreateDecompress) (j_decompress_ptr cinfo, int version, size_t structsize);
    void (*jpeg_destroy_decompress) (j_decompress_ptr cinfo);
    boolean (*jpeg_finish_decompress) (j_decompress_ptr cinfo);
    int (*jpeg_read_header) (j_decompress_ptr cinfo, boolean require_image);
    JDIMENSION (*jpeg_read_scanlines) (j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines);
    boolean (*jpeg_resync_to_restart) (j_decompress_ptr cinfo, int desired);
    boolean (*jpeg_start_decompress) (j_decompress_ptr cinfo);
    void (*jpeg_CreateCompress) (j_compress_ptr cinfo, int version, size_t structsize);
    void (*jpeg_start_compress) (j_compress_ptr cinfo, boolean write_all_tables);
    void (*jpeg_set_quality) (j_compress_ptr cinfo, int quality, boolean force_baseline);
    void (*jpeg_set_defaults) (j_compress_ptr cinfo);
    JDIMENSION (*jpeg_write_scanlines) (j_compress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION num_lines);
    void (*jpeg_finish_compress) (j_compress_ptr cinfo);
    void (*jpeg_destroy_compress) (j_compress_ptr cinfo);
    struct jpeg_error_mgr * (*jpeg_std_error) (struct jpeg_error_mgr * err);
} lib;

#ifdef LOAD_JPG_DYNAMIC
#define FUNCTION_LOADER(FUNC, SIG) \
    lib.FUNC = (SIG) SDL_LoadFunction(lib.handle, #FUNC); \
    if (lib.FUNC == NULL) { SDL_UnloadObject(lib.handle); return false; }
#else
#define FUNCTION_LOADER(FUNC, SIG) \
    lib.FUNC = FUNC;
#endif

static bool IMG_InitJPG(void)
{
    if ( lib.loaded == 0 ) {
#ifdef LOAD_JPG_DYNAMIC
        lib.handle = SDL_LoadObject(LOAD_JPG_DYNAMIC);
        if ( lib.handle == NULL ) {
            return false;
        }
#endif
        FUNCTION_LOADER(jpeg_calc_output_dimensions, void (*) (j_decompress_ptr cinfo))
        FUNCTION_LOADER(jpeg_CreateDecompress, void (*) (j_decompress_ptr cinfo, int version, size_t structsize))
        FUNCTION_LOADER(jpeg_destroy_decompress, void (*) (j_decompress_ptr cinfo))
        FUNCTION_LOADER(jpeg_finish_decompress, boolean (*) (j_decompress_ptr cinfo))
        FUNCTION_LOADER(jpeg_read_header, int (*) (j_decompress_ptr cinfo, boolean require_image))
        FUNCTION_LOADER(jpeg_read_scanlines, JDIMENSION (*) (j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines))
        FUNCTION_LOADER(jpeg_resync_to_restart, boolean (*) (j_decompress_ptr cinfo, int desired))
        FUNCTION_LOADER(jpeg_start_decompress, boolean (*) (j_decompress_ptr cinfo))
        FUNCTION_LOADER(jpeg_CreateCompress, void (*) (j_compress_ptr cinfo, int version, size_t structsize))
        FUNCTION_LOADER(jpeg_start_compress, void (*) (j_compress_ptr cinfo, boolean write_all_tables))
        FUNCTION_LOADER(jpeg_set_quality, void (*) (j_compress_ptr cinfo, int quality, boolean force_baseline))
        FUNCTION_LOADER(jpeg_set_defaults, void (*) (j_compress_ptr cinfo))
        FUNCTION_LOADER(jpeg_write_scanlines, JDIMENSION (*) (j_compress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION num_lines))
        FUNCTION_LOADER(jpeg_finish_compress, void (*) (j_compress_ptr cinfo))
        FUNCTION_LOADER(jpeg_destroy_compress, void (*) (j_compress_ptr cinfo))
        FUNCTION_LOADER(jpeg_std_error, struct jpeg_error_mgr * (*) (struct jpeg_error_mgr * err))
    }
    ++lib.loaded;

    return true;
}

#if 0
void IMG_QuitJPG(void)
{
    if ( lib.loaded == 0 ) {
        return;
    }
    if ( lib.loaded == 1 ) {
#ifdef LOAD_JPG_DYNAMIC
        SDL_UnloadObject(lib.handle);
#endif
    }
    --lib.loaded;
}
#endif // 0

/* See if an image is contained in a data source */
bool IMG_isJPG(SDL_IOStream *src)
{
    Sint64 start;
    bool is_JPG;
    bool in_scan;
    Uint8 magic[4];

    /* This detection code is by Steaphan Greene <stea@cs.binghamton.edu> */
    /* Blame me, not Sam, if this doesn't work right. */
    /* And don't forget to report the problem to the the sdl list too! */

    if (!src) {
        return false;
    }

    start = SDL_TellIO(src);
    is_JPG = false;
    in_scan = false;
    if (SDL_ReadIO(src, magic, 2) == 2) {
        if ( (magic[0] == 0xFF) && (magic[1] == 0xD8) ) {
            is_JPG = true;
            while (is_JPG) {
                if (SDL_ReadIO(src, magic, 2) != 2) {
                    is_JPG = false;
                } else if ( (magic[0] != 0xFF) && !in_scan ) {
                    is_JPG = false;
                } else if ( (magic[0] != 0xFF) || (magic[1] == 0xFF) ) {
                    /* Extra padding in JPEG (legal) */
                    /* or this is data and we are scanning */
                    SDL_SeekIO(src, -1, SDL_IO_SEEK_CUR);
                } else if (magic[1] == 0xD9) {
                    /* Got to end of good JPEG */
                    break;
                } else if ( in_scan && (magic[1] == 0x00) ) {
                    /* This is an encoded 0xFF within the data */
                } else if ( (magic[1] >= 0xD0) && (magic[1] < 0xD9) ) {
                    /* These have nothing else */
                } else if (SDL_ReadIO(src, magic+2, 2) != 2) {
                    is_JPG = false;
                } else {
                    /* Yes, it's big-endian */
                    Sint64 innerStart;
                    Uint32 size;
                    Sint64 end;
                    innerStart = SDL_TellIO(src);
                    size = (magic[2] << 8) + magic[3];
                    end = SDL_SeekIO(src, size-2, SDL_IO_SEEK_CUR);
                    if ( end != innerStart + size - 2 ) {
                        is_JPG = false;
                    }
                    if ( magic[1] == 0xDA ) {
                        /* Now comes the actual JPEG meat */
#ifdef  FAST_IS_JPEG
                        /* Ok, I'm convinced.  It is a JPEG. */
                        break;
#else
                        /* I'm not convinced.  Prove it! */
                        in_scan = true;
#endif
                    }
                }
            }
        }
    }
    SDL_SeekIO(src, start, SDL_IO_SEEK_SET);
    return is_JPG;
}

#define INPUT_BUFFER_SIZE   4096
typedef struct {
    struct jpeg_source_mgr pub;

    SDL_IOStream *ctx;
    Uint8 buffer[INPUT_BUFFER_SIZE];
} my_source_mgr;

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */
static void init_source (j_decompress_ptr cinfo)
{
    /* We don't actually need to do anything */
    (void)cinfo;
    return;
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 */
static boolean fill_input_buffer (j_decompress_ptr cinfo)
{
    my_source_mgr * src = (my_source_mgr *) cinfo->src;
    size_t nbytes;

    nbytes = SDL_ReadIO(src->ctx, src->buffer, INPUT_BUFFER_SIZE);
    if (nbytes == 0) {
        /* Insert a fake EOI marker */
        src->buffer[0] = (Uint8) 0xFF;
        src->buffer[1] = (Uint8) JPEG_EOI;
        nbytes = 2;
    }
    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;

    return TRUE;
}


/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 */
static void skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
    my_source_mgr * src = (my_source_mgr *) cinfo->src;

    /* Just a dumb implementation for now.  Could use fseek() except
     * it doesn't work on pipes.  Not clear that being smart is worth
     * any trouble anyway --- large skips are infrequent.
     */
    if (num_bytes > 0) {
        while (num_bytes > (long) src->pub.bytes_in_buffer) {
            num_bytes -= (long) src->pub.bytes_in_buffer;
            (void) src->pub.fill_input_buffer(cinfo);
            /* note we assume that fill_input_buffer will never
             * return FALSE, so suspension need not be handled.
             */
        }
        src->pub.next_input_byte += (size_t) num_bytes;
        src->pub.bytes_in_buffer -= (size_t) num_bytes;
    }
}

/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.
 */
static void term_source (j_decompress_ptr cinfo)
{
    /* We don't actually need to do anything */
    (void)cinfo;
    return;
}

/*
 * Prepare for input from a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing decompression.
 */
static void jpeg_SDL_IO_src (j_decompress_ptr cinfo, SDL_IOStream *ctx)
{
  my_source_mgr *src;

  /* The source object and input buffer are made permanent so that a series
   * of JPEG images can be read from the same file by calling jpeg_stdio_src
   * only before the first one.  (If we discarded the buffer at the end of
   * one image, we'd likely lose the start of the next one.)
   * This makes it unsafe to use this manager and a different source
   * manager serially with the same JPEG object.  Caveat programmer.
   */
  if (cinfo->src == NULL) { /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                  sizeof(my_source_mgr));
    src = (my_source_mgr *) cinfo->src;
  }

  src = (my_source_mgr *) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = lib.jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = term_source;
  src->ctx = ctx;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */
}

struct my_error_mgr {
    struct jpeg_error_mgr errmgr;
    jmp_buf escape;
};

static void my_error_exit(j_common_ptr cinfo)
{
    struct my_error_mgr *err = (struct my_error_mgr *)cinfo->err;
    longjmp(err->escape, 1);
}

static void output_no_message(j_common_ptr cinfo)
{
    /* do nothing */
    (void)cinfo;
}

struct loadjpeg_vars {
    const char *error;
    SDL_Surface *surface;
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
};

/* Load a JPEG type image from an SDL datasource */
static bool LIBJPEG_LoadJPG_IO(SDL_IOStream *src, struct loadjpeg_vars *vars)
{
    JSAMPROW rowptr[1];

    /* Create a decompression structure and load the JPEG header */
    vars->cinfo.err = lib.jpeg_std_error(&vars->jerr.errmgr);
    vars->jerr.errmgr.error_exit = my_error_exit;
    vars->jerr.errmgr.output_message = output_no_message;
    if (setjmp(vars->jerr.escape)) {
        /* If we get here, libjpeg found an error */
        lib.jpeg_destroy_decompress(&vars->cinfo);
        vars->error = "JPEG loading error";
        return false;
    }

    lib.jpeg_create_decompress(&vars->cinfo);
    jpeg_SDL_IO_src(&vars->cinfo, src);
    lib.jpeg_read_header(&vars->cinfo, TRUE);

    if (vars->cinfo.num_components == 4) {
        /* Set 32-bit Raw output */
        vars->cinfo.out_color_space = JCS_CMYK;
        vars->cinfo.quantize_colors = FALSE;
        lib.jpeg_calc_output_dimensions(&vars->cinfo);

        /* Allocate an output surface to hold the image */
        vars->surface = SDL_CreateSurface(vars->cinfo.output_width, vars->cinfo.output_height, SDL_PIXELFORMAT_BGRA32);
    } else {
        /* Set 24-bit RGB output */
        vars->cinfo.out_color_space = JCS_RGB;
        vars->cinfo.quantize_colors = FALSE;
#ifdef FAST_JPEG
        vars->cinfo.scale_num   = 1;
        vars->cinfo.scale_denom = 1;
        vars->cinfo.dct_method = JDCT_FASTEST;
        vars->cinfo.do_fancy_upsampling = FALSE;
#endif
        lib.jpeg_calc_output_dimensions(&vars->cinfo);

        /* Allocate an output surface to hold the image */
        vars->surface = SDL_CreateSurface(vars->cinfo.output_width, vars->cinfo.output_height, SDL_PIXELFORMAT_RGB24);
    }

    if (!vars->surface) {
        lib.jpeg_destroy_decompress(&vars->cinfo);
        return false;
    }

    /* Decompress the image */
    lib.jpeg_start_decompress(&vars->cinfo);
    while (vars->cinfo.output_scanline < vars->cinfo.output_height) {
        rowptr[0] = (JSAMPROW)(Uint8 *)vars->surface->pixels +
                            vars->cinfo.output_scanline * vars->surface->pitch;
        lib.jpeg_read_scanlines(&vars->cinfo, rowptr, (JDIMENSION) 1);
    }
    lib.jpeg_finish_decompress(&vars->cinfo);
    lib.jpeg_destroy_decompress(&vars->cinfo);

    return true;
}

SDL_Surface *IMG_LoadJPG_IO(SDL_IOStream *src)
{
    Sint64 start;
    struct loadjpeg_vars vars;

    if (!src) {
        /* The error message has been set in SDL_IOFromFile */
        return NULL;
    }

    if (!IMG_InitJPG()) {
        return NULL;
    }

    start = SDL_TellIO(src);
    SDL_zero(vars);

    if (LIBJPEG_LoadJPG_IO(src, &vars)) {
        return vars.surface;
    }

    /* this may clobber a set error if seek fails: don't care. */
    SDL_SeekIO(src, start, SDL_IO_SEEK_SET);
    if (vars.surface) {
        SDL_DestroySurface(vars.surface);
    }
    if (vars.error) {
        SDL_SetError("%s", vars.error);
    }

    return NULL;
}

#define OUTPUT_BUFFER_SIZE   4096
typedef struct {
    struct jpeg_destination_mgr pub;

    SDL_IOStream *ctx;
    Uint8 buffer[OUTPUT_BUFFER_SIZE];
} my_destination_mgr;

static void init_destination(j_compress_ptr cinfo)
{
    /* We don't actually need to do anything */
    (void)cinfo;
    return;
}

static boolean empty_output_buffer(j_compress_ptr cinfo)
{
    my_destination_mgr * dest = (my_destination_mgr *)cinfo->dest;

    /* In typical applications, it should write out the *entire* buffer */
    SDL_WriteIO(dest->ctx, dest->buffer, OUTPUT_BUFFER_SIZE);
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUFFER_SIZE;

    return TRUE;
}

static void term_destination(j_compress_ptr cinfo)
{
    my_destination_mgr * dest = (my_destination_mgr *)cinfo->dest;

    /*  In most applications, this must flush any data remaining in the buffer */
    SDL_WriteIO(dest->ctx, dest->buffer, OUTPUT_BUFFER_SIZE - dest->pub.free_in_buffer);
}

static void jpeg_SDL_IO_dest(j_compress_ptr cinfo, SDL_IOStream *ctx)
{
    my_destination_mgr *dest;

    if (cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)
            (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_PERMANENT,
            sizeof(my_destination_mgr));
        dest = (my_destination_mgr *)cinfo->dest;
    }

    dest = (my_destination_mgr *)cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->ctx = ctx;
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUFFER_SIZE;
}

struct savejpeg_vars
{
    struct jpeg_compress_struct cinfo;
    struct my_error_mgr jerr;
    Sint64 original_offset;
};

static bool JPEG_SaveJPEG_IO(struct savejpeg_vars *vars, SDL_Surface *jpeg_surface, SDL_IOStream *dst, int quality)
{
    /* Create a compression structure and load the JPEG header */
    vars->cinfo.err = lib.jpeg_std_error(&vars->jerr.errmgr);
    vars->jerr.errmgr.error_exit = my_error_exit;
    vars->jerr.errmgr.output_message = output_no_message;
    vars->original_offset = SDL_TellIO(dst);

    if (setjmp(vars->jerr.escape)) {
        /* If we get here, libjpeg found an error */
        lib.jpeg_destroy_compress(&vars->cinfo);
        SDL_SeekIO(dst, vars->original_offset, SDL_IO_SEEK_SET);
        return SDL_SetError("Error saving JPEG with libjpeg");
    }

    lib.jpeg_create_compress(&vars->cinfo);
    jpeg_SDL_IO_dest(&vars->cinfo, dst);

    vars->cinfo.image_width = jpeg_surface->w;
    vars->cinfo.image_height = jpeg_surface->h;
    vars->cinfo.in_color_space = JCS_RGB;
    vars->cinfo.input_components = 3;

    lib.jpeg_set_defaults(&vars->cinfo);
    lib.jpeg_set_quality(&vars->cinfo, quality, TRUE);
    lib.jpeg_start_compress(&vars->cinfo, TRUE);

    while (vars->cinfo.next_scanline < vars->cinfo.image_height) {
        JSAMPROW row_pointer[1];
        int offset = vars->cinfo.next_scanline * jpeg_surface->pitch;

        row_pointer[0] = ((Uint8*)jpeg_surface->pixels) + offset;
        lib.jpeg_write_scanlines(&vars->cinfo, row_pointer, 1);
    }

    lib.jpeg_finish_compress(&vars->cinfo);
    lib.jpeg_destroy_compress(&vars->cinfo);
    return true;
}

static bool IMG_SaveJPG_IO_jpeglib(SDL_Surface *surface, SDL_IOStream *dst, int quality)
{
    /* The JPEG library reads bytes in R,G,B order, so this is the right
     * encoding for either endianness */
    struct savejpeg_vars vars;
    static const Uint32 jpg_format = SDL_PIXELFORMAT_RGB24;
    SDL_Surface* jpeg_surface = surface;
    bool result;

    if (!IMG_InitJPG()) {
        return false;
    }

    /* Convert surface to format we can save */
    if (surface->format != jpg_format) {
        jpeg_surface = SDL_ConvertSurface(surface, jpg_format);
        if (!jpeg_surface) {
            return false;
        }
    }

    SDL_zero(vars);
    result = JPEG_SaveJPEG_IO(&vars, jpeg_surface, dst, quality);

    if (jpeg_surface != surface) {
        SDL_DestroySurface(jpeg_surface);
    }
    return result;
}

#elif defined(USE_STBIMAGE)

extern SDL_Surface *IMG_LoadSTB_IO(SDL_IOStream *src);

/* FIXME: This is a copypaste from JPEGLIB! Pull that out of the ifdefs */
/* Define this for quicker (but less perfect) JPEG identification */
#define FAST_IS_JPEG
/* See if an image is contained in a data source */
bool IMG_isJPG(SDL_IOStream *src)
{
    Sint64 start;
    bool is_JPG;
    bool in_scan;
    Uint8 magic[4];

    /* This detection code is by Steaphan Greene <stea@cs.binghamton.edu> */
    /* Blame me, not Sam, if this doesn't work right. */
    /* And don't forget to report the problem to the the sdl list too! */

    if (!src) {
        return false;
    }

    start = SDL_TellIO(src);
    is_JPG = false;
    in_scan = false;
    if (SDL_ReadIO(src, magic, 2) == 2) {
        if ( (magic[0] == 0xFF) && (magic[1] == 0xD8) ) {
            is_JPG = true;
            while (is_JPG) {
                if (SDL_ReadIO(src, magic, 2) != 2) {
                    is_JPG = false;
                } else if ( (magic[0] != 0xFF) && !in_scan ) {
                    is_JPG = false;
                } else if ( (magic[0] != 0xFF) || (magic[1] == 0xFF) ) {
                    /* Extra padding in JPEG (legal) */
                    /* or this is data and we are scanning */
                    SDL_SeekIO(src, -1, SDL_IO_SEEK_CUR);
                } else if (magic[1] == 0xD9) {
                    /* Got to end of good JPEG */
                    break;
                } else if ( in_scan && (magic[1] == 0x00) ) {
                    /* This is an encoded 0xFF within the data */
                } else if ( (magic[1] >= 0xD0) && (magic[1] < 0xD9) ) {
                    /* These have nothing else */
                } else if (SDL_ReadIO(src, magic+2, 2) != 2) {
                    is_JPG = false;
                } else {
                    /* Yes, it's big-endian */
                    Sint64 innerStart;
                    Uint32 size;
                    Sint64 end;
                    innerStart = SDL_TellIO(src);
                    size = (magic[2] << 8) + magic[3];
                    end = SDL_SeekIO(src, size-2, SDL_IO_SEEK_CUR);
                    if ( end != innerStart + size - 2 ) {
                        is_JPG = false;
                    }
                    if ( magic[1] == 0xDA ) {
                        /* Now comes the actual JPEG meat */
#ifdef  FAST_IS_JPEG
                        /* Ok, I'm convinced.  It is a JPEG. */
                        break;
#else
                        /* I'm not convinced.  Prove it! */
                        in_scan = true;
#endif
                    }
                }
            }
        }
    }
    SDL_SeekIO(src, start, SDL_IO_SEEK_SET);
    return is_JPG;
}

/* Load a JPEG type image from an SDL datasource */
SDL_Surface *IMG_LoadJPG_IO(SDL_IOStream *src)
{
    return IMG_LoadSTB_IO(src);
}

#endif /* WANT_JPEGLIB */

#else

/* See if an image is contained in a data source */
bool IMG_isJPG(SDL_IOStream *src)
{
    (void)src;
    return false;
}

/* Load a JPEG type image from an SDL datasource */
SDL_Surface *IMG_LoadJPG_IO(SDL_IOStream *src)
{
    (void)src;
    return NULL;
}

#endif /* LOAD_JPG */

/* Use tinyjpeg as a fallback if we don't have a hard dependency on libjpeg */
#if SDL_IMAGE_SAVE_JPG && (defined(LOAD_JPG_DYNAMIC) || !defined(WANT_JPEGLIB))

#ifdef assert
#undef assert
#endif
#ifdef memcpy
#undef memcpy
#endif
#ifdef memset
#undef memset
#endif
#define assert SDL_assert
#define memcpy SDL_memcpy
#define memset SDL_memset

#define ceilf SDL_ceilf
#define floorf SDL_floorf
#define cosf SDL_cosf

#define tje_log SDL_Log
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

static void IMG_SaveJPG_IO_tinyjpeg_callback(void* context, void* data, int size)
{
    SDL_WriteIO((SDL_IOStream*) context, data, size);
}

static bool IMG_SaveJPG_IO_tinyjpeg(SDL_Surface *surface, SDL_IOStream *dst, int quality)
{
    /* The JPEG library reads bytes in R,G,B order, so this is the right
     * encoding for either endianness */
    static const Uint32 jpg_format = SDL_PIXELFORMAT_RGB24;
    SDL_Surface* jpeg_surface = surface;
    bool result = false;

    /* Convert surface to format we can save */
    if (surface->format != jpg_format) {
        jpeg_surface = SDL_ConvertSurface(surface, jpg_format);
        if (!jpeg_surface) {
            return false;
        }
    }

    /* Quality for tinyjpeg is from 1-3:
     * 0  - 33  - Lowest quality
     * 34 - 66  - Middle quality
     * 67 - 100 - Highest quality
     */
    if      (quality < 34) quality = 1;
    else if (quality < 67) quality = 2;
    else                   quality = 3;

    result = tje_encode_with_func(
        IMG_SaveJPG_IO_tinyjpeg_callback,
        dst,
        quality,
        jpeg_surface->w,
        jpeg_surface->h,
        3,
        jpeg_surface->pixels,
        jpeg_surface->pitch
    );

    if (jpeg_surface != surface) {
        SDL_DestroySurface(jpeg_surface);
    }

    if (!result) {
        SDL_SetError("tinyjpeg error");
    }
    return result;
}

#endif /* SDL_IMAGE_SAVE_JPG && (defined(LOAD_JPG_DYNAMIC) || !defined(WANT_JPEGLIB)) */

bool IMG_SaveJPG(SDL_Surface *surface, const char *file, int quality)
{
    SDL_IOStream *dst = SDL_IOFromFile(file, "wb");
    if (dst) {
        return IMG_SaveJPG_IO(surface, dst, 1, quality);
    } else {
        return false;
    }
}

bool IMG_SaveJPG_IO(SDL_Surface *surface, SDL_IOStream *dst, bool closeio, int quality)
{
    bool result = false;
    (void)surface;
    (void)quality;

    if (!dst) {
        return SDL_SetError("Passed NULL dst");
    }

#if SDL_IMAGE_SAVE_JPG
#ifdef USE_JPEGLIB
    if (!result) {
        result = IMG_SaveJPG_IO_jpeglib(surface, dst, quality);
    }
#endif

#if defined(LOAD_JPG_DYNAMIC) || !defined(WANT_JPEGLIB)
    if (!result) {
        result = IMG_SaveJPG_IO_tinyjpeg(surface, dst, quality);
    }
#endif

#else
    result = SDL_SetError("SDL_image built without JPEG save support");
#endif

    if (closeio) {
        SDL_CloseIO(dst);
    }
    return result;
}

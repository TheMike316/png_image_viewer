#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define KiB 1 << 10
#define MiB 1 << 20

#define OK 0
#define ERROR (-1)

#define IMG_COMPRESSION_DEBUG
#define IMG_PIXEL_DEBUG

const unsigned char png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};

/*
 * PNG chunk types
 */
// const uint32_t CHUNK_TYPE_IHDR = 0x49484452;
// const unsigned char CHUNK_TYPE_IHDR[] = {73, 72, 48, 64};
const unsigned char CHUNK_TYPE_IHDR[] = {'I', 'H', 'D', 'R'};
// const uint32_t CHUNK_TYPE_IEND = 0x49454E44;
// const unsigned char CHUNK_TYPE_IEND[] = {73, 64, 65, 78};
const unsigned char CHUNK_TYPE_IEND[] = {'I', 'E', 'N', 'D'};
// const uint32_t CHUNK_TYPE_IDAT = 0x49444154;
// const unsigned char CHUNK_TYPE_IDAT[] = {73, 64, 61, 84};
const unsigned char CHUNK_TYPE_IDAT[] = {'I', 'D', 'A', 'T'};
// const uint32_t CHUNK_TYPE_PLTE = 0x504C5445;
// const unsigned char CHUNK_TYPE_PLTE[] = {80, 76, 84, 65};
const unsigned char CHUNK_TYPE_PLTE[] = {'P', 'L', 'T', 'E'};
// const uint32_t CHUNK_TYPE_bKGD = 0x624B4744;
const unsigned char CHUNK_TYPE_bKGD[] = {'b', 'K', 'G', 'D'};
// const uint32_t CHUNK_TYPE_bKGD = 0x624B4744;
// const uint32_t CHUNK_TYPE_pHYs = 0x70485973;
const unsigned char CHUNK_TYPE_pHYs[] = {'p', 'H', 'Y', 's'};
// const uint32_t CHUNK_TYPE_tIME = 0x74494D45;
const unsigned char CHUNK_TYPE_tIME[] = {'t', 'I', 'M', 'E'};

/*
 * interlacing starts and steps per pass
 */
const size_t STARTS_X[7] = {0, 4, 0, 2, 0, 1, 0};
const size_t STARTS_Y[7] = {0, 0, 4, 0, 2, 0, 1};
const size_t STEPS_X[7] = {8, 8, 4, 4, 2, 2, 1};
const size_t STEPS_Y[7] = {8, 8, 8, 4, 4, 2, 2};

const size_t PASS_WIDTH_FACTORS[7] = {1, 1, 2, 2, 4, 4, 8};
const size_t PASS_HEIGHT_FACTORS[7] = {1, 1, 1, 2, 2, 4, 4};

bool arrays_equal(const unsigned char *a, const unsigned char *b, const size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

uint32_t convert_bytes_to_uint32(const unsigned char *bytes) {
    return ((uint32_t) bytes[0] << 24) | ((uint32_t) bytes[1] << 16) | ((uint32_t) bytes[2] << 8) |
           ((uint32_t) bytes[3]);
}

typedef struct {
    size_t len;
    size_t cap;
    unsigned char *buf;
} char_buffer;

int init(char_buffer *arr) {
    assert(arr && "arr not initialized");
    assert(!arr->buf && "already initialized!");

    arr->cap = MiB;
    arr->buf = (unsigned char *) malloc(arr->cap);
    if (!arr->buf) {
        return -1;
    }
    arr->len = 0;

    return 0;
}

int append(char_buffer *arr, const unsigned char *input, const size_t input_len) {
    assert(input && "invalid null input");
    bool resize = false;
    while (arr->len + input_len >= arr->cap) {
        arr->cap *= 2;
        resize = true;
    }
    if (resize) {
        unsigned char *new_buf = realloc(arr->buf, arr->cap);
        if (!new_buf) {
            return -1;
        }
        arr->buf = new_buf;
    }

    for (size_t i = 0; i < input_len; ++i) {
        arr->buf[arr->len++] = input[i];
    }
    return 0;
}

int append_single(char_buffer *arr, const unsigned char input) {
    if (arr->len + 1 >= arr->cap) {
        arr->cap *= 2;
        unsigned char *new_buf = realloc(arr->buf, arr->cap);
        if (!new_buf) {
            return -1;
        }
        arr->buf = new_buf;
    }
    arr->buf[arr->len++] = input;
    return 0;
}

int set_at(char_buffer *arr, const size_t i, const unsigned char input) {
    assert(arr && "invalid null arr");
    while (i >= arr->cap) {
        arr->cap *= 2;
        unsigned char *new_buf = realloc(arr->buf, arr->cap);
        if (!new_buf) {
            return -1;
        }
        arr->buf = new_buf;
    }
    arr->buf[i] = input;
    if (i >= arr->len) {
        arr->len = i + 1;
    }
    return 0;
}

// todo error handling
void reset(char_buffer *arr) {
    assert(arr && "arr not initialized");
    assert(arr->buf && "no memory allocated");
    arr->len = 0;
}

void destroy(char_buffer *arr) {
    if (!arr) {
        return;
    }

    free(arr->buf);
    arr->buf = NULL;
    arr->len = 0;
    arr->cap = 0;
}

typedef struct {
    const unsigned char *buf;
    size_t len;
} char_view;

char_view sub(const char_buffer *buf, const size_t start_pos, const size_t len) {
    if (start_pos + len > buf->len) {
        const char_view resp = {0, 0};
        return resp;
    }
    const char_view resp = {buf->buf + start_pos, len};
    return resp;
}

char_view sub_view(const unsigned char *buf, const size_t start_pos, const size_t len, const size_t max_len) {
    const size_t _len = start_pos + len > max_len ? max_len - start_pos : len;
    const char_view resp = {buf + start_pos, _len};
    return resp;
}

int png_extract_uncompressed_data(const char *filename, char *out_buf, const size_t out_len) {
    assert(filename && "filename not initialized");
    assert(out_buf && "out_buf not initialized");

    FILE *fptr = fopen(filename, "r");

    if (!fptr) {
        return ERROR;
    }

    char_buffer file_array = {};
    if (init(&file_array) < 0) {
        return ERROR;
    }

    char buffer[KiB] = {0};
    while (fgets(buffer, sizeof buffer, fptr)) {
        const size_t str_len = strnlen(buffer, sizeof buffer);
        // todo how to safely and properly convert from char to unsigned char?
        if (append(&file_array, buffer, str_len) < 0) {
            return ERROR;
        }
    }

    // check signature
    // todo check signature earlier
    if (file_array.len < sizeof(png_sig)) {
        return ERROR;
    }
    int sig_matches = 1;
    for (size_t i = 0; i < sizeof(png_sig); ++i) {
        if (png_sig[i] != file_array.buf[i]) {
            sig_matches = 0;
            break;
        }
    }
    if (!sig_matches) {
        return ERROR;
    }

    size_t file_offset = sizeof(png_sig);
    // unzip file payload
    // TODO make scratch arenas
    unsigned char *compressed = file_array.buf + file_offset;

    z_stream zlib_stream = {0};
    inflateInit(&zlib_stream);
    zlib_stream.next_in = (Bytef *) compressed;
    zlib_stream.avail_in = file_array.len - file_offset;
    zlib_stream.next_out = (Bytef *) out_buf;
    zlib_stream.avail_out = out_len;

    inflate(&zlib_stream, Z_NO_FLUSH);
    inflateEnd(&zlib_stream);

    return OK;
}
// parse png image in stb_image style returning a raw pixel array
unsigned char *load_image(const char *filename, int *out_width, int *out_height, size_t *out_len) {
    FILE *fptr = fopen(filename, "rb");

    if (!fptr) {
        return NULL;
    }

    // todo parse png in one go; for now it's easier to have the file fully in memory
    char_buffer file_array = {};
    if (init(&file_array) < 0) {
        return NULL;
    }

    unsigned char buffer[KiB] = {0};
    size_t bytes_read = 0;
    while ((bytes_read = fread(buffer, sizeof(unsigned char), sizeof buffer, fptr)) > 0) {
        if (append(&file_array, buffer, bytes_read) < 0) {
            return NULL;
        }
    }

    // check signature
    if (file_array.len < sizeof(png_sig)) {
        return NULL;
    }
    int sig_matches = 1;
    for (size_t i = 0; i < sizeof(png_sig); ++i) {
        if (png_sig[i] != file_array.buf[i]) {
            sig_matches = 0;
            break;
        }
    }
    if (!sig_matches) {
        return NULL;
    }

    size_t file_offset = sizeof(png_sig);
    while (file_offset < file_array.len - 4) {
        if (file_array.buf[file_offset] == CHUNK_TYPE_IHDR[0]) {
            break;
        }
        file_offset++;
    }
    if (file_offset + 4 == file_array.len) {
        printf("shiiit\n");
        return NULL;
    }

    // first chunk MUST be header i.e. IHDR. this makes the parsing slightly easier
    // 1. read 4 bytes for chunk type
    unsigned char type_buf[4] = {0};
    for (size_t i = 0; i < 4; ++i) {
        type_buf[i] = file_array.buf[file_offset++];
    }
    if (arrays_equal(type_buf, CHUNK_TYPE_IHDR, sizeof type_buf)) {
        printf("header found\n");
    }

    // header layout
    // Width:              4 bytes
    // Height:             4 bytes
    // Bit depth:          1 byte
    // Color type:         1 byte
    // Compression method: 1 byte
    // Filter method:      1 byte
    // Interlace method:   1 byte

    // width
    for (size_t i = 0; i < 4; ++i) {
        type_buf[i] = file_array.buf[file_offset++];
    }
    *out_width = type_buf[0] << 24 | type_buf[1] << 16 | type_buf[2] << 8 | type_buf[3];
    // height
    for (size_t i = 0; i < 4; ++i) {
        type_buf[i] = file_array.buf[file_offset++];
    }
    *out_height = type_buf[0] << 24 | type_buf[1] << 16 | type_buf[2] << 8 | type_buf[3];

    // ignore other header stuff for now
    unsigned char bit_depth = file_array.buf[file_offset++];
    unsigned char color_type = file_array.buf[file_offset++];
    unsigned char comp_method = file_array.buf[file_offset++];
    unsigned char filter_method = file_array.buf[file_offset++];
    unsigned char interlace_method = file_array.buf[file_offset++];

    printf("bit depth: %d\n", bit_depth);
    printf("color type: %d\n", color_type);
    printf("comp method: %d\n", comp_method);
    printf("filter method: %d\n", filter_method);
    printf("interlace method: %d\n", interlace_method);

    char_buffer compressed_buffer = {};
    if (init(&compressed_buffer) < 0) {
        return NULL;
    }

    while (file_offset < file_array.len) {
        // search IDAT chunks and add payload to compressed buffer
        if (file_array.buf[file_offset] == CHUNK_TYPE_IDAT[0] &&
            file_array.buf[file_offset + 1] == CHUNK_TYPE_IDAT[1] &&
            file_array.buf[file_offset + 2] == CHUNK_TYPE_IDAT[2] &&
            file_array.buf[file_offset + 3] == CHUNK_TYPE_IDAT[3]) {
            // the length are the 4 bytes preceeding the chunk type
            for (size_t i = 4; i > 0; --i) {
                type_buf[4 - i] = file_array.buf[file_offset - i];
            }
            const uint32_t data_length = type_buf[0] << 24 | type_buf[1] << 16 | type_buf[2] << 8 | type_buf[3];
            // jump ahead to data
            file_offset += 4;
            for (size_t i = 0; i < data_length; ++i) {
                append_single(&compressed_buffer, file_array.buf[file_offset++]);
            }
            // skip CRC for now
            file_offset += 4;
            // technically i could look out for IEND but this is good enough for now
        } else {
            file_offset++;
        }
    }

#ifdef IMG_COMPRESSION_DEBUG
    FILE *debug_compressed_file = fopen("./compressed.bin", "wb");
    const size_t decompressed_n =
            fwrite(compressed_buffer.buf, sizeof(unsigned char), compressed_buffer.len, debug_compressed_file);
    printf("decompressed %zu bytes\n", decompressed_n);
    fclose(debug_compressed_file);
#endif

    // todo better buffer; i know it's not freed everywhere
    unsigned char *uncompressed = malloc(10 * MiB);
    size_t uncompressed_len = 10 * MiB;
    int uncompress_return = Z_OK;
    if ((uncompress_return =
                 uncompress(uncompressed, &uncompressed_len, compressed_buffer.buf, compressed_buffer.len)) != Z_OK) {
        printf("uncompress return: %d\n", uncompress_return);
        return NULL;
    }

    printf("actual uncompressed length: %zu bytes\n", uncompressed_len);

    // this comparison is not true for adam7 interlaced png
    // const size_t expected_size = *out_height * (*out_width * 4 + 1);
    // assert(uncompressed_len == expected_size && "expected size does not match decompressed size");

    // color type 6 is rgb with alpha,

    // because we need to add the pixels
    char_buffer pixels_array = {};
    if (init(&pixels_array) < 0) {
        return NULL;
    }

    printf("img width %d\n", *out_width);
    printf("img height %d\n", *out_height);

    // TODO code adam7 completely separate first, then look where things can be refactored

    if (interlace_method == 0) {
        const size_t stride = *out_width * 4; // 4 bytes for color type 6; todo handle per color type
        const size_t row_len = stride + 1; // 1 filter byte at the start of the row

        printf("stride %lu\n", stride);
        printf("row_len %lu\n", row_len);

        // bytes per pixel; depends on color type, for 6 it is 4
        unsigned char *cur = NULL;
        const unsigned char *prev = NULL;

        for (size_t row_num = 0; row_num < *out_height; ++row_num) {
            unsigned char *row = uncompressed + row_num * row_len;

            const int filter = row[0];
            cur = row + 1;

            assert(*row <= 4 && "filter byte is not in 0-4 range");
            unsigned char up = 0;
            unsigned char left = 0;
            unsigned char up_left = 0;
            for (size_t x = 0; x < stride; ++x) {
                const short bpp = 4;
                up = prev ? prev[x] : 0;
                left = x >= bpp ? cur[x - bpp] : 0;
                up_left = prev && x >= bpp ? prev[x - bpp] : 0;
                const unsigned char raw = cur[x];
                unsigned char recon;
                switch (filter) {
                    case 0:
                        // NONE. append bytes as-is
                        recon = raw;
                        break;
                    case 1:
                        // SUB
                        recon = raw + left & 0xFF;
                        break;
                    case 2:
                        // UP
                        recon = raw + up & 0xFF;
                        break;
                    case 3:
                        // AVG
                        recon = raw + ((left + up) >> 1) & 0xFF;
                        break;
                    case 4:
                        // PAETH
                        // predict closest pixel
                        int p = left + up - up_left;
                        int pa = abs(p - left);
                        int pb = abs(p - up);
                        int pc = abs(p - up_left);

                        int pr;
                        if (pa <= pb && pa <= pc) {
                            pr = left;
                        } else if (pb <= pa && pb <= pc) {
                            pr = up;
                        } else {
                            pr = up_left;
                        }
                        recon = raw + pr & 0xFF;
                        break;
                    default:
                        // todo error handling
                        printf("invalid filter type: %d", filter);
                        recon = raw;
                }

                cur[x] = recon;
            }

            append(&pixels_array, cur, stride);
            prev = cur;
        }
    } else if (interlace_method == 1) {
        /*
         * adam7
         */

        unsigned char *row_ptr = uncompressed;

        for (size_t pass = 0; pass < 7; ++pass) {
            printf("pass %lu\n", pass + 1);
            const size_t bpp = 4;
            const size_t start_x = STARTS_X[pass];
            const size_t start_y = STARTS_Y[pass];
            const size_t step_x = STEPS_X[pass];
            const size_t step_y = STEPS_Y[pass];
            //
            // const size_t pass_width = (*out_width - start_x + step_x - 1) / step_x;
            // const size_t pass_height = (*out_height - start_y + step_y - 1) / step_y;
            //
            //
            // // debug printing
            // printf("pass %lu; start_x %lu; start_y %lu; step_x %lu; step_y %lu; pass_x %lu; pass_y %lu\n", pass,
            //        start_x, start_y, step_x, step_y, pass_width, pass_height);
            //
            // if (pass_width <= 0 || pass_height <= 0) {
            //     // skipping
            //     continue;
            // }
            //
            // const size_t stride = pass_width * bpp;
            // const size_t row_len = stride + 1;
            const size_t width_factor = PASS_WIDTH_FACTORS[pass];
            const size_t height_factor = PASS_HEIGHT_FACTORS[pass];

            const size_t stride = width_factor * *out_width * bpp / 8;
            const size_t row_len = stride + 1;

            // const size_t pass_height = height_factor * *out_height / 8;
            const size_t pass_height = (*out_height - start_y + step_y - 1) / step_y;

            unsigned char *prev = NULL;
            unsigned char *cur = NULL;

            // for (size_t y = start_y; y < *out_height; y += step_y) {
            // we need to track y offset
            size_t final_y_pos = start_y * *out_width * bpp;
            for (size_t y = 0; y < pass_height; ++y) {

                unsigned char filter = row_ptr[0];

                // assert(filter <= 4 && "filter byte is not in 0-4 range");

                cur = row_ptr + 1;

                unsigned char up = 0;
                unsigned char left = 0;
                unsigned char up_left = 0;

                // we use this to track x offset in final array
                size_t final_x_pos = start_x * bpp;
                for (size_t x = 0; x < stride; ++x) {
                    const unsigned char raw = cur[x];
                    up = prev ? prev[x] : 0;
                    left = x >= bpp ? cur[x - bpp] : 0;
                    up_left = prev && x >= bpp ? cur[x - bpp] : 0;

                    unsigned char recon;
                    switch (filter) {
                        case 0:
                            // NONE
                            recon = raw;
                            break;
                        case 1:
                            // SUB
                            recon = raw + left & 0xFF;
                            break;
                        case 2:
                            // up
                            recon = raw + up & 0xFF;
                            break;
                        case 3:
                            // AVG
                            recon = raw + ((left + up) >> 1) & 0xFF;
                            break;
                        case 4:
                            // PAETH
                            const int p = left + up - up_left;
                            int pa = abs(p - left);
                            int pb = abs(p - up);
                            int pc = abs(p - up_left);

                            int pr;
                            if (pa <= pb && pa <= pc) {
                                pr = left;
                            } else if (pb <= pa && pb <= pc) {
                                pr = up;
                            } else {
                                pr = up_left;
                            }
                            recon = raw + pr & 0xFF;
                            break;
                        default:
                            printf("invalid filter type: %hhu\n", filter);
                            recon = raw;
                    }


                    const size_t final_buff_pos = final_x_pos + final_y_pos;
                    set_at(&pixels_array, final_buff_pos, recon);

                    // we need to paint bpp bytes before moving on to the next pixel
                    if ((x + 1) % bpp == 0) {
                        final_x_pos += step_x * bpp - bpp + 1;
                    } else {
                        final_x_pos++;
                    }
                    cur[x] = recon;
                }
                final_y_pos += step_y * *out_width * bpp;
                prev = cur;

                row_ptr += row_len;
            }
        }
    } else {
        printf("invalid interlace method: %hhu", interlace_method);
        return NULL;
    }

    unsigned char *result_pixels = malloc(pixels_array.len);
    memcpy(result_pixels, pixels_array.buf, pixels_array.len);

    *out_len = pixels_array.len;

    free(uncompressed);
    fclose(fptr);
    destroy(&file_array);
    destroy(&compressed_buffer);
    destroy(&pixels_array);

    return result_pixels;
}

void free_image(unsigned char *image) { free(image); }


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *filename = argv[1];

    int width, height;
    size_t pixel_len;
    unsigned char *pixels = load_image(filename, &width, &height, &pixel_len);

    if (!pixels) {
        printf("error loading image\n");
        return EXIT_FAILURE;
    }

    printf("width: %d; height: %d\n", width, height);

#ifdef IMG_PIXEL_DEBUG
    FILE *pixelFile = fopen("./pixels.rgba", "wb");
    assert(pixelFile);
    fwrite(pixels, pixel_len, 1, pixelFile);
    fclose(pixelFile);
#endif

    // raylib fun

    InitWindow(width, height, "png image viewer");

    const Image rawImage = {.width = width,
                            .height = height,
                            .data = pixels,
                            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                            .mipmaps = 1};

    const Texture2D tex = LoadTextureFromImage(rawImage);

    while (!WindowShouldClose()) {
        BeginDrawing();

        DrawTexture(tex, 0, 0, WHITE);
        EndDrawing();
    }

    free_image(pixels);

    return EXIT_SUCCESS;
}

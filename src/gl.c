#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glew.h>

#include "errors.h"
#include "gl.h"

static void GL_CompileShader(GLuint shader, const char *source_path)
{
    FILE *file = fopen(source_path, "r");
    if (file == NULL) {
        Error_Raise(FATAL, ERR_IO, strerror(errno));
    }

    // Find the size of the file.
    fseek(file, 0, SEEK_END);
    GLint file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = Malloc(file_size);
    fread(source, 1, file_size, file);
    fclose(file);

    // Compile the shader
    glShaderSource(shader, 1, (const char * const *)&source, &file_size);
    glCompileShader(shader);
    free(source);

    // Check compilation
    GLint result = GL_FALSE;
    int err_length;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &err_length);
    if (err_length > 0) {
        char *err_msg = Malloc(err_length + 1);
        glGetShaderInfoLog(shader, err_length, NULL, err_msg);
        Error_Raise(FATAL, ERR_INVALID_SHADER, err_msg);
    }
}

GLuint GL_LoadShaders(const char *vertex_path, const char *fragment_path)
{

    // Create the shaders
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    // Compile them
    GL_CompileShader(vertex_shader, vertex_path);
    GL_CompileShader(fragment_shader, fragment_path);

    // Link the program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    // Check the program
    GLint result = GL_FALSE;
    int err_length;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &err_length);
    if (err_length > 0) {
        char *err_msg = Malloc(err_length + 1);
        glGetProgramInfoLog(program, err_length, NULL, err_msg);
        Error_Raise(FATAL, ERR_INVALID_SHADER, err_msg);
    }

    // Clean up
    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

typedef struct __attribute__((__packed__)) {
    // File header
    char magic_number[2];   // Must be "BM"
    uint32_t file_size;
    uint8_t reserved1[2];
    uint8_t reserved2[2];
    uint32_t data_offset;

    // DIB header
    uint32_t header_size;   // Size of this header. This is typically used to
                            // figure out which version of the header we're
                            // working with, as later versions add lots of extra
                            // features after the core DIB header. We don't care
                            // about this header, so we'll just ignore this
                            // field and only use the fields which are common to
                            // all versions.
    uint32_t width;         // Width in pixels.
    uint32_t height;        // Height in pixels;
    uint16_t num_color_planes;
                            // Always 1
    uint16_t bit_depth;
    uint32_t compression_method;
    uint32_t image_size;

} BmpHeader;

typedef enum {
    BMP_PIXEL_RGBA,
    BMP_PIXEL_UNSUPPORTED,
} BmpPixelLayout;

static void BMP_ReadHeader(BmpHeader *header, FILE *bmp)
{
    fread(header, 1, sizeof(*header), bmp);
    if (header->magic_number[0] != 'B' ||
        header->magic_number[1] != 'M') {
        Error_Raise(FATAL, ERR_IO, "invalid bitmap texture");
    }

    trace("Read bitmap header:"
          "    size:        %u\n"
          "    data offset: 0x%x\n"
          "    header size: %u\n"
          "    image size:  %ux%u\n"
          "    bit depth:   %u\n"
          "    compression: %u\n",
        header->file_size, header->data_offset,
        header->header_size, header->width, header->height,
        header->bit_depth, header->compression_method
    );

    // Validate the header.
    switch (header->bit_depth) {
        case 8:
        case 16:
        case 24:
        case 32:
            break;
        default:
            Error_Raise(FATAL, ERR_IO, "invalid bitmap bit depth");
    }
}

static uint8_t *BMP_ReadData(const BmpHeader *header, FILE *bmp)
    // This function allocates space for the pixel data using `Malloc`, remember
    // to free the result when finished with it.
{
    // Find the start of the data.
    uint32_t data_offset = header->data_offset
                            ? header->data_offset
                            : header->header_size;
    fseek(bmp, data_offset, SEEK_SET);

    // Compute the size of the image. The `image_size` field in the header is
    // not always reliable, sometimes it is incorrectly 0.
    assert(header->bit_depth % 8 == 0);
    uint32_t data_size =
        header->image_size
            ? header->image_size
            : header->width*header->height*(header->bit_depth/8);

    // Read in the data.
    uint8_t *pixels = Malloc(data_size);
    fread(pixels, 1, data_size, bmp);
    return pixels;
}

static BmpPixelLayout BMP_GetPixelLayout(const BmpHeader *header)
{
    if (header->compression_method == 3 &&
            // Compression method 3 can indicate one of several things,
            // depending on header version:
            //  * OS22X header: Huffman 1D compression
            //  * BITMAPV2 header: RGB bit field masks
            //  * BITMAPV3+ header: RGBA 32-bit encoding
            // Of these, we only support the RGBA encoding, so we next check if
            // this header is V3+.
        header->header_size >= 56
            // Bitmap header version is indicated by the size of the header;
            // The size increases with the version. The V3 header is 56 bytes,
            // subsequent headers are larger.
        ) {
        return BMP_PIXEL_RGBA;
    }

    return BMP_PIXEL_UNSUPPORTED;
}

GLuint GL_LoadTexture(const char *bmp_path)
{
    FILE *file = fopen(bmp_path, "rb");
    if (file == NULL) {
        Error_Raise(FATAL, ERR_IO, strerror(errno));
    }

    // Read in the header.
    BmpHeader header;
    BMP_ReadHeader(&header, file);

    // Bring the pixel data into memory.
    uint8_t *data = BMP_ReadData(&header, file);

    // All the data is in memory now, we're done with the file.
    fclose(file);

    // Give the data to GL.
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    {
        // Format-specific processing.
        switch (BMP_GetPixelLayout(&header)) {
            case BMP_PIXEL_RGBA: {
                assert(header.bit_depth == 32);
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGBA8,
                    header.width,
                    header.height,
                    0,
                    GL_BGRA,
                    GL_UNSIGNED_INT_8_8_8_8,
                    data
                );
            }
            break;

            default:
                Error_Raise(FATAL, ERR_IO, "unsupported bitmap pixel format");
        }

        // Use linear filtering to interpolate between texels.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    free(data);

    return texture;
}

#include "return_codes.h"
#include <stdio.h>
#include <stdlib.h>
#define ZLIB
#if defined(ZLIB)
#include <zlib.h>
#define library 1
#elif (defined(LIBDEFLATE))
#include <libdeflate.h>
#define library 2
#elif (defined(ISAL))
#include <include/igzip_lib.h>
#define library 3
#else
#error "This library not supported"
#endif

struct chunk {
    uLongf dataLen;// first 4 bytes
    // int type;   // second 4 bytes
    unsigned char *data;// third dateLen bytes
    int notFound;
    // maybe crc for fix error, but now without it. (last 4 bytes)
    int massivCreate;
};
unsigned char readByte(FILE *fin, int *code_error, int ischecker) {
    int argsGet;
    unsigned char symbol;
    argsGet = fscanf(fin, "%c", &symbol);
    if (argsGet != 1) {
        if (!ischecker)
            fprintf(stderr, "Can't find some bytes");
        *code_error = ERROR_INVALID_DATA;
    }
    return symbol;
}
void skeepBytes(FILE *fin, unsigned int n, int *code_error) {
    for (unsigned int i = 0; i < n; i++) {
        readByte(fin, code_error, 0);
        if (*code_error != 0) {
            return;
        }
    }
}
void isPNG(FILE *fin, int *code_error) {
    const unsigned char magicNumbers[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    unsigned char PNGbyteNow;
    for (int i = 0; i < 8; i++) {
        PNGbyteNow = readByte(fin, code_error, 0);
        if (*code_error != 0)
            return;
        if (PNGbyteNow != magicNumbers[i]) {
            fprintf(stderr, "It's not png");
            *code_error = ERROR_INVALID_DATA;
            return;
        }
    }
}
void createData(FILE *fin, struct chunk *name, unsigned int length, int *code_error) {
    name->notFound = 0;
    name->dataLen = length;
    name->massivCreate = 0;
    if (!(name->data = malloc(length * sizeof(unsigned char)))) {
        *code_error = ERROR_NOT_ENOUGH_MEMORY;
        return;
    } else {
        name->massivCreate = 1;
        for (unsigned int i = 0; i < length; i++) {
            name->data[i] = readByte(fin, code_error, 0);
            if (*code_error != 0) {
                return;
            }
        }
    }
}
void chunksFounder(FILE *fin, struct chunk *IHDR, struct chunk *IDAT, struct chunk *IEND, int *code_error) {
    const unsigned char IHDRname[] = "IHDR";
    const unsigned char IDATname[] = "IDAT";
    const unsigned char IENDname[] = "IEND";
    while (1) {
        unsigned char symbol;
        unsigned int length = 0;
        unsigned int alpha = 256 * 256 * 256;
        symbol = readByte(fin, code_error, 1);
        if (*code_error == ERROR_INVALID_DATA) {
            *code_error = ERROR_SUCCESS;
            return;
        }
        length = length + symbol * alpha;
        for (int i = 0; i < 3; i++) {
            alpha = alpha / 256;
            symbol = readByte(fin, code_error, 0);
            if (*code_error != 0)
                return;
            length = length + symbol * alpha;
        }

        // upper this will be all cleanest ---------------------------
        unsigned char *name;
        if (!(name = malloc(4 * sizeof(unsigned char)))) {
            fprintf(stderr, "Clean memory pls. It's not enough memory");
            *code_error = ERROR_NOT_ENOUGH_MEMORY;
            return;
        }
        for (int i = 0; i < 4; i++) {
            name[i] = readByte(fin, code_error, 0);
            if (*code_error != 0) {
                free(name);
                return;
            }
        }

        int isIt = 1, someOfIs = 0;
        for (int i = 0; i < 4; i++)
            if (name[i] != IHDRname[i])
                isIt = 0;
        if (isIt == 1) {
            createData(fin, IHDR, length, code_error);
            if (*code_error != 0) {
                free(name);
                return;
            }
            someOfIs = 1;
        }

        isIt = 1;
        for (int i = 0; i < 4; i++)
            if (name[i] != IDATname[i])
                isIt = 0;
        if (isIt == 1) {
            createData(fin, IDAT, length, code_error);
            if (*code_error != 0) {
                free(name);
                return;
            }
            someOfIs = 1;
        }

        isIt = 1;
        for (int i = 0; i < 4; i++)
            if (name[i] != IENDname[i])
                isIt = 0;
        if (isIt == 1) {
            IEND->notFound = 0;
            IEND->dataLen = length;
            someOfIs = 1;
        }
        if (someOfIs == 0) {
            skeepBytes(fin, length, code_error);
            if (*code_error != 0) {
                free(name);
                return;
            }
        }
        free(name);
        skeepBytes(fin, 4, code_error);
        if (*code_error != 0) {
            return;
        }
    }
}
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Need 2 arguments");
        return ERROR_INVALID_PARAMETER;
    }
    FILE *fin = fopen(argv[1], "rb");
    if (fin == NULL) {
        fprintf(stderr, "Input file not found and can't create");
        return ERROR_FILE_NOT_FOUND;
    }
    int code_error = 0;
    isPNG(fin, &code_error);
    if (code_error != 0) {
        fclose(fin);
        return code_error;
    }
    struct chunk IHDR, IDAT, IEND;
    IHDR.notFound = 1;
    IDAT.notFound = 1;
    IEND.notFound = 1;
    chunksFounder(fin, &IHDR, &IDAT, &IEND, &code_error);
    if (code_error != 0) {
        fclose(fin);
        if (IHDR.massivCreate == 1)
            free(IHDR.data);
        if (IDAT.massivCreate == 1)
            free(IDAT.data);
        return code_error;
    }
    if (IHDR.notFound == 1 || IDAT.notFound == 1 || IEND.notFound == 1) {
        fprintf(stderr, "It's PNG but i not find main field");
        fclose(fin);
        if (IHDR.massivCreate == 1)
            free(IHDR.data);
        if (IDAT.massivCreate == 1)
            free(IDAT.data);
        return ERROR_INVALID_DATA;
    }
    fclose(fin);
    // we will check IHDR size
    if (IHDR.dataLen != 13) {
        free(IHDR.data);
        free(IDAT.data);
        fprintf(stderr, "It's bad PNG file");
        return ERROR_INVALID_DATA;
    }

    unsigned int height = 0, width = 0;
    unsigned int alpha = 256 * 256 * 256;
    for (int i = 0; i < 4; i++) {
        width += alpha * IHDR.data[i];
        alpha = alpha / 256;
    }

    alpha = 256 * 256 * 256;
    for (int i = 4; i < 8; i++) {
        height += alpha * IHDR.data[i];
        alpha = alpha / 256;
    }
    int colourType = IHDR.data[9] + 1;
    unsigned char *image;

    uLongf decompressed_size = height * (width + 1) * colourType;
    printf("%d", decompressed_size);
    if (!(image = malloc(height * (width + 1) * colourType * sizeof(unsigned char)))) {
        fprintf(stderr, "Out of memory. clean pls");
    }

    long long len = height * width * colourType;
    // after this we need decompose IDATA.data and write file, but now i've big troubles with it and will fix it next
    // patch.
    int err;
    err = uncompress(image, &decompressed_size, IDAT.data, IDAT.dataLen);
    if(err!=Z_OK){
        printf("sosi");
    }
    free(image);
    free(IHDR.data);
    free(IDAT.data);
    return 0;
}
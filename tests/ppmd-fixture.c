// SPDX-License-Identifier: LGPL-3.0-or-later
// Test-only container writer; compression is entirely official 7-Zip Ppmd8.
#include "Ppmd8.h"
#include <stdio.h>
#include <stdlib.h>
static void *allocate(ISzAllocPtr allocator, size_t size) { (void)allocator; return malloc(size); }
static void release(ISzAllocPtr allocator, void *address) { (void)allocator; free(address); }
typedef struct { IByteOut interface; FILE *file; } Output;
static void writeByte(IByteOutPtr pointer, Byte value) { Output *output = (Output *)pointer; fputc(value, output->file); }
int main(int argc, char **argv) {
    if (argc != 3) return 1;
    FILE *input = fopen(argv[1], "rb"), *output = fopen(argv[2], "wb"); if (!input || !output) return 1;
    const unsigned char header[] = {0x8f,0xaf,0xac,0x84,0,0,0,0,5,0x80,11,0,0,0,0,0};
    if (fwrite(header, 1, sizeof(header), output) != sizeof(header) || fwrite("payload.txt",1,11,output) != 11) return 1;
    ISzAlloc allocator = {allocate, release}; CPpmd8 model; Ppmd8_Construct(&model);
    if (!Ppmd8_Alloc(&model, 1 << 20, &allocator)) return 1;
    Output stream = {{writeByte}, output}; model.Stream.Out = &stream.interface;
    Ppmd8_Init_RangeEnc(&model); Ppmd8_Init(&model, 6, PPMD8_RESTORE_METHOD_RESTART);
    int value; while ((value = fgetc(input)) != EOF) Ppmd8_EncodeSymbol(&model, value);
    Ppmd8_EncodeSymbol(&model, -1); Ppmd8_Flush_RangeEnc(&model); Ppmd8_Free(&model, &allocator);
    const int error = ferror(input) || ferror(output); fclose(input); const int closeError = fclose(output);
    return error || closeError ? 1 : 0;
}
